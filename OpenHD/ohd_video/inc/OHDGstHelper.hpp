//
// Created by consti10 on 15.05.22.
//

#ifndef OPENHD_OHDGSTHELPER_H
#define OPENHD_OHDGSTHELPER_H

#include <gst/gst.h>
#include "openhd-camera.hpp"
#include <sstream>
#include <string>
#include <fmt/format.h>

/**
 * Helper methods to create parts of gstreamer pipes.
 * Note: Unless a pipeline part is used at the end, all pipelines should end with '! '.
 * This way we avoid syntax errors.
 */
namespace OHDGstHelper{
    /**
     * Check if we can find gstreamer at run time, throw a runtime error if not.
     */
    static void initGstreamerOrThrow(){
        GError* error = nullptr;
        if (!gst_init_check(nullptr, nullptr, &error)) {
            std::cerr << "gst_init_check() failed: " << error->message << std::endl;
            g_error_free(error);
            throw std::runtime_error("GStreamer initialization failed");
        }
    }

    // a createXXXStream function always ends wth "h164,h265 or mjpeg stream ! "
    // aka after that, onc can add a rtp encoder or similar.
    // All these methods also start from zero - aka have a source like videotestsrc, nvarguscamerasr usw in the beginning
    // and end with a OpenHD supported video codec (e.g. h264,h265 or mjpeg)
    // ------------- crateXXXStream begin -------------
    /**
     * Create a encoded dummy stream for the selected video format, that means a stream that encodes data coming from a videotestsrc.
     */
    static std::string createDummyStream(const VideoFormat videoFormat){
        // TODO: create dummies for h265 and jpeg
        assert(videoFormat.videoCodec==VideoCodecH264);
        std::stringstream ss;
        ss<<"videotestsrc ! ";
        ss<<fmt::format("video/x-raw, format=NV12,width={},height={},framerate={}/1 ! ",videoFormat.width,videoFormat.height,videoFormat.framerate);
        if(videoFormat.videoCodec==VideoCodecH264){
            ss<<fmt::format("x264enc bitrate={} tune=zerolatency key-int-max=10 ! ",DEFAULT_BITRATE_KBITS);
        }else if(videoFormat.videoCodec==VideoCodecH265){
            ss<<fmt::format("x265enc name=encodectrl bitrate={} ! ", DEFAULT_BITRATE_KBITS);
        }else{
            std::cerr<<"no sw encoder for MJPEG,cannot start dummy camera\n";
        }
        return ss.str();
    }

    /**
     * Create a encoded stream for rpicamsrc, which supports h264 only.
     */
    static std::string createRpicamsrcStream(const std::string& bus,const int bitrate,const VideoFormat videoFormat){
        assert(videoFormat.isValid());
        assert(videoFormat.videoCodec==VideoCodecH264);
        std::stringstream ss;
        ss<< fmt::format("rpicamsrc name=bitratectrl camera-number={} bitrate={} preview=0 ! ",bus,bitrate);
        ss<< fmt::format("video/x-h264, profile=constrained-baseline, width={}, height={}, framerate={}/1, level=3.0 ! ",
                                  videoFormat.width, videoFormat.height,videoFormat.framerate);
        return ss.str();
    }

    /**
     * Create a encoded stream for the jetson, which is fully hardware accelerated for h264,h265 and mjpeg.
     */
    static std::string createJetsonStream(const int sensor_id,const int bitrate,const VideoFormat videoFormat){
        assert(videoFormat.videoCodec!=VideoCodecUnknown);
        std::stringstream ss;
        ss << fmt::format("nvarguscamerasrc do-timestamp=true sensor-id={} ! ", sensor_id);
        ss << fmt::format("video/x-raw(memory:NVMM), width={}, height={}, format=NV12, framerate={}/1 ! ",
                                  videoFormat.width, videoFormat.height,videoFormat.framerate);
        if (videoFormat.videoCodec == VideoCodecH265) {
            ss << fmt::format("nvv4l2h265enc name=vnenc control-rate=1 insert-sps-pps=1 bitrate={} ! ",bitrate);
        } else if(videoFormat.videoCodec==VideoCodecH264){
            ss << fmt::format("nvv4l2h264enc name=nvenc control-rate=1 insert-sps-pps=1 bitrate={} ! ",bitrate);
        }else  {
            ss<< fmt::format("nvjpegenc quality=50 ! ");
        }
        return ss.str();
    }

     /**
      * For V4l2 Cameras that do raw YUV (or RGB) we use a sw encoder.
      * This one has no custom resolution(s) yet.
      */
     static std::string createV4l2SrcRawAndSwEncodeStream(const std::string& device_node, const VideoCodec videoCodec, const int bitrate){
         std::stringstream ss;
         assert(videoCodec!=VideoCodecUnknown);
         ss << fmt::format("v4l2src name=picturectrl device={} ! ", device_node);
         // rn we omit the set resolution/framerate here and let gstreamer figure it out.
         // TODO: do it better ;)
         std::cout<< "Allowing gstreamer to choose UVC format" << std::endl;
         ss << fmt::format("video/x-raw ! ");
         ss << "videoconvert ! ";
         // Add a queue here. With sw we are not low latency anyways.
         ss<<"queue ! ";
         assert(videoCodec!=VideoCodecUnknown);
         if(videoCodec==VideoCodecH264){
             ss<<fmt::format("x264enc name=encodectrl bitrate={} tune=zerolatency key-int-max=10 ! ", bitrate);
         }else if(videoCodec==VideoCodecH265){
             ss<<fmt::format("x265enc name=encodectrl bitrate={} ! ", bitrate);
         }else{
             std::cerr<<"no sw encoder for MJPEG\n";
         }
         return ss.str();
     }

     /**
      * This one is for v4l2src cameras that outputs already encoded video.
      */
     static std::string createV4l2SrcAlreadyEncodedStream(const std::string& device_node, const VideoFormat videoFormat){
         std::stringstream ss;
         assert(videoFormat.videoCodec!=VideoCodecUnknown);
         ss<< fmt::format("v4l2src name=picturectrl device={} ! ", device_node);
         if(videoFormat.videoCodec==VideoCodecH264){
             ss<< fmt::format("video/x-h264, width={}, height={}, framerate={}/1 ! ",
                              videoFormat.width, videoFormat.height,videoFormat.framerate);
         }else if(videoFormat.videoCodec==VideoCodecH265){
             ss<< fmt::format("video/x-h265, width={}, height={}, framerate={}/1 ! ",
                              videoFormat.width, videoFormat.height,videoFormat.framerate);
         }else{
             assert(videoFormat.videoCodec==VideoCodecMJPEG);
             ss << fmt::format("image/jpeg, width={}, height={}, framerate={}/1 ! ",
                                       videoFormat.width, videoFormat.height,videoFormat.framerate);
         }
         return ss.str();
     }

    // These are not tested
    static std::string createUVCH264Stream(const std::string& device_node,const int bitrate,const VideoFormat videoFormat){
        assert(videoFormat.videoCodec==VideoCodecH264);
        std::stringstream ss;
        ss << fmt::format("uvch264src device={} peak-bitrate={} initial-bitrate={} average-bitrate={} rate-control=1 iframe-period=1000 name=encodectrl auto-start=true encodectrl.vidsrc ! ", device_node,bitrate,bitrate,bitrate);
        ss << fmt::format("video/x-h264,width={}, height={}, framerate={}/1 ! ",
                                  videoFormat.width,videoFormat.height,videoFormat.framerate);
        return ss.str();
    }
    static std::string createIpCameraStream(const std::string& url){
        std::stringstream ss;
        // none of the other params are used at the moment, we would need to set them with ONVIF or a per-camera API of some sort,
        // however most people seem to set them in advance with the proprietary app that came with the camera anyway
        ss << fmt::format("rtspsrc location=\"{}\" latency=0 ! ", url);
        return ss.str();
    }
    // ------------- crateXXXStream end  -------------

    /**
    * Create the part of the pipeline that takes the raw h264/h265/mjpeg from gstreamer and packs it into rtp.
    * @param videoCodec the video codec o create the rtp for.
    * @return the gstreamer pipeline part.
    */
    static std::string createRtpForVideoCodec(const VideoCodec videoCodec){
        std::stringstream ss;
        assert(videoCodec!=VideoCodecUnknown);
        if(videoCodec==VideoCodecH264){
            ss << "h264parse config-interval=-1 ! ";
            ss << "rtph264pay mtu=1024 ! ";
        }else if(videoCodec==VideoCodecH265){
            ss << "h265parse config-interval=-1 ! ";
            ss << "rtph265pay mtu=1024 ! ";
        }else{
            assert(videoCodec==VideoCodecMJPEG);
            // mjpeg
            ss << "jpegparse config-interval=-1 ! ";
            ss << "rtpjpegpay mtu=1024 ! ";
        }
        return ss.str();
    }

    /**
    * Create the part of the pipeline that takes the rtp from gstreamer and sends it to udp.
    * @param udpOutPort the udp (localhost) port.
    * @return the gstreamer pipeline part
    */
    static std::string createOutputUdpLocalhost(const int udpOutPort){
        return fmt::format(" udpsink host=127.0.0.1 port={} ", udpOutPort);
    }
}
#endif //OPENHD_OHDGSTHELPER_H
