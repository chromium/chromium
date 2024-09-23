// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/mojom/video_accelerator_mojom_traits.h"

namespace mojo {

// Make sure values in arc::mojom::VideoCodecProfile match to the values in
// media::VideoCodecProfile.
#define CHECK_PROFILE_ENUM(value)                                             \
  static_assert(                                                              \
      static_cast<int>(arc::mojom::VideoCodecProfile::value) == media::value, \
      "enum ##value mismatch")

CHECK_PROFILE_ENUM(VIDEO_CODEC_PROFILE_UNKNOWN);
CHECK_PROFILE_ENUM(VIDEO_CODEC_PROFILE_MIN);
CHECK_PROFILE_ENUM(H264PROFILE_MIN);
CHECK_PROFILE_ENUM(H264PROFILE_BASELINE);
CHECK_PROFILE_ENUM(H264PROFILE_MAIN);
CHECK_PROFILE_ENUM(H264PROFILE_EXTENDED);
CHECK_PROFILE_ENUM(H264PROFILE_HIGH);
CHECK_PROFILE_ENUM(H264PROFILE_HIGH10PROFILE);
CHECK_PROFILE_ENUM(H264PROFILE_HIGH422PROFILE);
CHECK_PROFILE_ENUM(H264PROFILE_HIGH444PREDICTIVEPROFILE);
CHECK_PROFILE_ENUM(H264PROFILE_SCALABLEBASELINE);
CHECK_PROFILE_ENUM(H264PROFILE_SCALABLEHIGH);
CHECK_PROFILE_ENUM(H264PROFILE_STEREOHIGH);
CHECK_PROFILE_ENUM(H264PROFILE_MULTIVIEWHIGH);
CHECK_PROFILE_ENUM(H264PROFILE_MAX);
CHECK_PROFILE_ENUM(VP8PROFILE_MIN);
CHECK_PROFILE_ENUM(VP8PROFILE_ANY);
CHECK_PROFILE_ENUM(VP8PROFILE_MAX);
CHECK_PROFILE_ENUM(VP9PROFILE_MIN);
CHECK_PROFILE_ENUM(VP9PROFILE_PROFILE0);
CHECK_PROFILE_ENUM(VP9PROFILE_PROFILE1);
CHECK_PROFILE_ENUM(VP9PROFILE_PROFILE2);
CHECK_PROFILE_ENUM(VP9PROFILE_PROFILE3);
CHECK_PROFILE_ENUM(VP9PROFILE_MAX);
CHECK_PROFILE_ENUM(HEVCPROFILE_MIN);
CHECK_PROFILE_ENUM(HEVCPROFILE_MAIN);
CHECK_PROFILE_ENUM(HEVCPROFILE_MAIN10);
CHECK_PROFILE_ENUM(HEVCPROFILE_MAIN_STILL_PICTURE);
CHECK_PROFILE_ENUM(HEVCPROFILE_MAX);
CHECK_PROFILE_ENUM(DOLBYVISION_PROFILE0);
CHECK_PROFILE_ENUM(DOLBYVISION_PROFILE5);
CHECK_PROFILE_ENUM(DOLBYVISION_PROFILE7);
CHECK_PROFILE_ENUM(DOLBYVISION_PROFILE8);
CHECK_PROFILE_ENUM(DOLBYVISION_PROFILE9);
CHECK_PROFILE_ENUM(THEORAPROFILE_MIN);
CHECK_PROFILE_ENUM(THEORAPROFILE_ANY);
CHECK_PROFILE_ENUM(THEORAPROFILE_MAX);
CHECK_PROFILE_ENUM(AV1PROFILE_MIN);
CHECK_PROFILE_ENUM(AV1PROFILE_PROFILE_MAIN);
CHECK_PROFILE_ENUM(AV1PROFILE_PROFILE_HIGH);
CHECK_PROFILE_ENUM(AV1PROFILE_PROFILE_PRO);
CHECK_PROFILE_ENUM(AV1PROFILE_MAX);
CHECK_PROFILE_ENUM(HEVCPROFILE_EXT_MIN);
CHECK_PROFILE_ENUM(HEVCPROFILE_REXT);
CHECK_PROFILE_ENUM(HEVCPROFILE_HIGH_THROUGHPUT);
CHECK_PROFILE_ENUM(HEVCPROFILE_MULTIVIEW_MAIN);
CHECK_PROFILE_ENUM(HEVCPROFILE_SCALABLE_MAIN);
CHECK_PROFILE_ENUM(HEVCPROFILE_3D_MAIN);
CHECK_PROFILE_ENUM(HEVCPROFILE_SCREEN_EXTENDED);
CHECK_PROFILE_ENUM(HEVCPROFILE_SCALABLE_REXT);
CHECK_PROFILE_ENUM(HEVCPROFILE_HIGH_THROUGHPUT_SCREEN_EXTENDED);
CHECK_PROFILE_ENUM(HEVCPROFILE_EXT_MAX);
CHECK_PROFILE_ENUM(VVCPROFILE_MIN);
CHECK_PROFILE_ENUM(VVCPROFILE_MAIN10);
CHECK_PROFILE_ENUM(VVCPROFILE_MAIN12);
CHECK_PROFILE_ENUM(VVCPROFILE_MAIN12_INTRA);
CHECK_PROFILE_ENUM(VVCPROIFLE_MULTILAYER_MAIN10);
CHECK_PROFILE_ENUM(VVCPROFILE_MAIN10_444);
CHECK_PROFILE_ENUM(VVCPROFILE_MAIN12_444);
CHECK_PROFILE_ENUM(VVCPROFILE_MAIN16_444);
CHECK_PROFILE_ENUM(VVCPROFILE_MAIN12_444_INTRA);
CHECK_PROFILE_ENUM(VVCPROFILE_MAIN16_444_INTRA);
CHECK_PROFILE_ENUM(VVCPROFILE_MULTILAYER_MAIN10_444);
CHECK_PROFILE_ENUM(VVCPROFILE_MAIN10_STILL_PICTURE);
CHECK_PROFILE_ENUM(VVCPROFILE_MAIN12_STILL_PICTURE);
CHECK_PROFILE_ENUM(VVCPROFILE_MAIN10_444_STILL_PICTURE);
CHECK_PROFILE_ENUM(VVCPROFILE_MAIN12_444_STILL_PICTURE);
CHECK_PROFILE_ENUM(VVCPROFILE_MAIN16_444_STILL_PICTURE);
CHECK_PROFILE_ENUM(VVCPROFILE_MAX);
CHECK_PROFILE_ENUM(VIDEO_CODEC_PROFILE_MAX);

#undef CHECK_PROFILE_ENUM

// static
arc::mojom::VideoCodecProfile
EnumTraits<arc::mojom::VideoCodecProfile, media::VideoCodecProfile>::ToMojom(
    media::VideoCodecProfile input) {
  return static_cast<arc::mojom::VideoCodecProfile>(input);
}

// static
bool EnumTraits<arc::mojom::VideoCodecProfile, media::VideoCodecProfile>::
    FromMojom(arc::mojom::VideoCodecProfile input,
              media::VideoCodecProfile* output) {
  switch (input) {
    case arc::mojom::VideoCodecProfile::VIDEO_CODEC_PROFILE_UNKNOWN:
    case arc::mojom::VideoCodecProfile::H264PROFILE_BASELINE:
    case arc::mojom::VideoCodecProfile::H264PROFILE_MAIN:
    case arc::mojom::VideoCodecProfile::H264PROFILE_EXTENDED:
    case arc::mojom::VideoCodecProfile::H264PROFILE_HIGH:
    case arc::mojom::VideoCodecProfile::H264PROFILE_HIGH10PROFILE:
    case arc::mojom::VideoCodecProfile::H264PROFILE_HIGH422PROFILE:
    case arc::mojom::VideoCodecProfile::H264PROFILE_HIGH444PREDICTIVEPROFILE:
    case arc::mojom::VideoCodecProfile::H264PROFILE_SCALABLEBASELINE:
    case arc::mojom::VideoCodecProfile::H264PROFILE_SCALABLEHIGH:
    case arc::mojom::VideoCodecProfile::H264PROFILE_STEREOHIGH:
    case arc::mojom::VideoCodecProfile::H264PROFILE_MULTIVIEWHIGH:
    case arc::mojom::VideoCodecProfile::VP8PROFILE_ANY:
    case arc::mojom::VideoCodecProfile::VP9PROFILE_PROFILE0:
    case arc::mojom::VideoCodecProfile::VP9PROFILE_PROFILE1:
    case arc::mojom::VideoCodecProfile::VP9PROFILE_PROFILE2:
    case arc::mojom::VideoCodecProfile::VP9PROFILE_PROFILE3:
    case arc::mojom::VideoCodecProfile::HEVCPROFILE_MAIN:
    case arc::mojom::VideoCodecProfile::HEVCPROFILE_MAIN10:
    case arc::mojom::VideoCodecProfile::HEVCPROFILE_MAIN_STILL_PICTURE:
    case arc::mojom::VideoCodecProfile::HEVCPROFILE_REXT:
    case arc::mojom::VideoCodecProfile::HEVCPROFILE_HIGH_THROUGHPUT:
    case arc::mojom::VideoCodecProfile::HEVCPROFILE_MULTIVIEW_MAIN:
    case arc::mojom::VideoCodecProfile::HEVCPROFILE_SCALABLE_MAIN:
    case arc::mojom::VideoCodecProfile::HEVCPROFILE_3D_MAIN:
    case arc::mojom::VideoCodecProfile::HEVCPROFILE_SCREEN_EXTENDED:
    case arc::mojom::VideoCodecProfile::HEVCPROFILE_SCALABLE_REXT:
    case arc::mojom::VideoCodecProfile::
        HEVCPROFILE_HIGH_THROUGHPUT_SCREEN_EXTENDED:
    case arc::mojom::VideoCodecProfile::DOLBYVISION_PROFILE0:
    case arc::mojom::VideoCodecProfile::DOLBYVISION_PROFILE5:
    case arc::mojom::VideoCodecProfile::DOLBYVISION_PROFILE7:
    case arc::mojom::VideoCodecProfile::DOLBYVISION_PROFILE8:
    case arc::mojom::VideoCodecProfile::DOLBYVISION_PROFILE9:
    case arc::mojom::VideoCodecProfile::THEORAPROFILE_ANY:
    case arc::mojom::VideoCodecProfile::AV1PROFILE_PROFILE_MAIN:
    case arc::mojom::VideoCodecProfile::AV1PROFILE_PROFILE_HIGH:
    case arc::mojom::VideoCodecProfile::AV1PROFILE_PROFILE_PRO:
    case arc::mojom::VideoCodecProfile::VVCPROFILE_MAIN10:
    case arc::mojom::VideoCodecProfile::VVCPROFILE_MAIN12:
    case arc::mojom::VideoCodecProfile::VVCPROFILE_MAIN12_INTRA:
    case arc::mojom::VideoCodecProfile::VVCPROIFLE_MULTILAYER_MAIN10:
    case arc::mojom::VideoCodecProfile::VVCPROFILE_MAIN10_444:
    case arc::mojom::VideoCodecProfile::VVCPROFILE_MAIN12_444:
    case arc::mojom::VideoCodecProfile::VVCPROFILE_MAIN16_444:
    case arc::mojom::VideoCodecProfile::VVCPROFILE_MAIN12_444_INTRA:
    case arc::mojom::VideoCodecProfile::VVCPROFILE_MAIN16_444_INTRA:
    case arc::mojom::VideoCodecProfile::VVCPROFILE_MULTILAYER_MAIN10_444:
    case arc::mojom::VideoCodecProfile::VVCPROFILE_MAIN10_STILL_PICTURE:
    case arc::mojom::VideoCodecProfile::VVCPROFILE_MAIN12_STILL_PICTURE:
    case arc::mojom::VideoCodecProfile::VVCPROFILE_MAIN10_444_STILL_PICTURE:
    case arc::mojom::VideoCodecProfile::VVCPROFILE_MAIN12_444_STILL_PICTURE:
    case arc::mojom::VideoCodecProfile::VVCPROFILE_MAIN16_444_STILL_PICTURE:
      *output = static_cast<media::VideoCodecProfile>(input);
      return true;
  }
  VLOG(1) << "unknown profile: "
          << media::GetProfileName(
                 static_cast<media::VideoCodecProfile>(input));
  return false;
}

// Make sure values in arc::mojom::VideoPixelFormat match to the values in
// media::VideoPixelFormat. The former is a subset of the later.
#define CHECK_PIXEL_FORMAT_ENUM(value)                                       \
  static_assert(                                                             \
      static_cast<int>(arc::mojom::VideoPixelFormat::value) == media::value, \
      "enum ##value mismatch")

CHECK_PIXEL_FORMAT_ENUM(PIXEL_FORMAT_UNKNOWN);
CHECK_PIXEL_FORMAT_ENUM(PIXEL_FORMAT_I420);
CHECK_PIXEL_FORMAT_ENUM(PIXEL_FORMAT_YV12);
CHECK_PIXEL_FORMAT_ENUM(PIXEL_FORMAT_NV12);
CHECK_PIXEL_FORMAT_ENUM(PIXEL_FORMAT_NV21);
CHECK_PIXEL_FORMAT_ENUM(PIXEL_FORMAT_ARGB);
CHECK_PIXEL_FORMAT_ENUM(PIXEL_FORMAT_ABGR);
CHECK_PIXEL_FORMAT_ENUM(PIXEL_FORMAT_XBGR);

#undef CHECK_PXIEL_FORMAT_ENUM

// static
arc::mojom::VideoPixelFormat
EnumTraits<arc::mojom::VideoPixelFormat, media::VideoPixelFormat>::ToMojom(
    media::VideoPixelFormat input) {
  switch (input) {
    case media::PIXEL_FORMAT_UNKNOWN:
    case media::PIXEL_FORMAT_I420:
    case media::PIXEL_FORMAT_YV12:
    case media::PIXEL_FORMAT_NV12:
    case media::PIXEL_FORMAT_NV21:
    case media::PIXEL_FORMAT_ARGB:
    case media::PIXEL_FORMAT_ABGR:
    case media::PIXEL_FORMAT_XBGR:
      return static_cast<arc::mojom::VideoPixelFormat>(input);

    default:
      NOTIMPLEMENTED();
      return arc::mojom::VideoPixelFormat::PIXEL_FORMAT_UNKNOWN;
  }
}

// static
bool EnumTraits<arc::mojom::VideoPixelFormat, media::VideoPixelFormat>::
    FromMojom(arc::mojom::VideoPixelFormat input,
              media::VideoPixelFormat* output) {
  switch (input) {
    case arc::mojom::VideoPixelFormat::PIXEL_FORMAT_UNKNOWN:
    case arc::mojom::VideoPixelFormat::PIXEL_FORMAT_I420:
    case arc::mojom::VideoPixelFormat::PIXEL_FORMAT_YV12:
    case arc::mojom::VideoPixelFormat::PIXEL_FORMAT_NV12:
    case arc::mojom::VideoPixelFormat::PIXEL_FORMAT_NV21:
    case arc::mojom::VideoPixelFormat::PIXEL_FORMAT_ARGB:
    case arc::mojom::VideoPixelFormat::PIXEL_FORMAT_ABGR:
    case arc::mojom::VideoPixelFormat::PIXEL_FORMAT_XBGR:
      *output = static_cast<media::VideoPixelFormat>(input);
      return true;
  }
  NOTREACHED();
}

// static
bool StructTraits<arc::mojom::VideoFramePlaneDataView, arc::VideoFramePlane>::
    Read(arc::mojom::VideoFramePlaneDataView data, arc::VideoFramePlane* out) {
  if (data.offset() < 0 || data.stride() < 0)
    return false;

  out->offset = data.offset();
  out->stride = data.stride();
  return true;
}

// static
bool StructTraits<arc::mojom::SizeDataView, gfx::Size>::Read(
    arc::mojom::SizeDataView data,
    gfx::Size* out) {
  if (data.width() < 0 || data.height() < 0)
    return false;

  out->SetSize(data.width(), data.height());
  return true;
}

// static
bool StructTraits<
    arc::mojom::ColorPlaneLayoutDataView,
    media::ColorPlaneLayout>::Read(arc::mojom::ColorPlaneLayoutDataView data,
                                   media::ColorPlaneLayout* out) {
  out->offset = data.offset();
  out->stride = data.stride();
  out->size = data.size();
  return true;
}

// static
bool StructTraits<arc::mojom::VideoFrameLayoutDataView,
                  std::unique_ptr<media::VideoFrameLayout>>::
    Read(arc::mojom::VideoFrameLayoutDataView data,
         std::unique_ptr<media::VideoFrameLayout>* out) {
  media::VideoPixelFormat format;
  gfx::Size coded_size;
  std::vector<media::ColorPlaneLayout> planes;
  if (!data.ReadFormat(&format) || !data.ReadCodedSize(&coded_size) ||
      !data.ReadPlanes(&planes)) {
    return false;
  }

  std::optional<media::VideoFrameLayout> layout =
      media::VideoFrameLayout::CreateWithPlanes(
          format, coded_size, std::move(planes), data.buffer_addr_align(),
          data.modifier());
  if (!layout)
    return false;

  *out = std::make_unique<media::VideoFrameLayout>(*layout);
  return true;
}

// static
arc::mojom::DecoderStatus
EnumTraits<arc::mojom::DecoderStatus, media::DecoderStatus>::ToMojom(
    media::DecoderStatus input) {
  switch (input.code()) {
    case media::DecoderStatus::Codes::kOk:
      return arc::mojom::DecoderStatus::OK;
    case media::DecoderStatus::Codes::kAborted:
      return arc::mojom::DecoderStatus::ABORTED;
    case media::DecoderStatus::Codes::kFailed:
      return arc::mojom::DecoderStatus::FAILED;
    case media::DecoderStatus::Codes::kFailedToCreateDecoder:
      return arc::mojom::DecoderStatus::CREATION_FAILED;
    case media::DecoderStatus::Codes::kInvalidArgument:
    case media::DecoderStatus::Codes::kUnsupportedConfig:
      return arc::mojom::DecoderStatus::INVALID_ARGUMENT;
    default:
      NOTREACHED() << "unknown status: " << static_cast<int>(input.code());
  }
}

// static
bool EnumTraits<arc::mojom::DecoderStatus, media::DecoderStatus>::FromMojom(
    arc::mojom::DecoderStatus input,
    media::DecoderStatus* output) {
  switch (input) {
    case arc::mojom::DecoderStatus::OK:
      *output = media::DecoderStatus::Codes::kOk;
      return true;
    case arc::mojom::DecoderStatus::ABORTED:
      *output = media::DecoderStatus::Codes::kAborted;
      return true;
    case arc::mojom::DecoderStatus::FAILED:
      *output = media::DecoderStatus::Codes::kFailed;
      return true;
    case arc::mojom::DecoderStatus::CREATION_FAILED:
      *output = media::DecoderStatus::Codes::kFailedToCreateDecoder;
      return true;
    case arc::mojom::DecoderStatus::INVALID_ARGUMENT:
      *output = media::DecoderStatus::Codes::kInvalidArgument;
      return true;
  }
  NOTREACHED() << "unknown status: " << static_cast<int>(input);
}

}  // namespace mojo
