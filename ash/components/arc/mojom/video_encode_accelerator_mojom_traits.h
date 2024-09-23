// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_MOJOM_VIDEO_ENCODE_ACCELERATOR_MOJOM_TRAITS_H_
#define ASH_COMPONENTS_ARC_MOJOM_VIDEO_ENCODE_ACCELERATOR_MOJOM_TRAITS_H_

#include "ash/components/arc/mojom/video_encode_accelerator.mojom-shared.h"
#include "media/base/bitrate.h"
#include "media/video/video_encode_accelerator.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/union_traits.h"

// Needs to be included after 'video_encode_accelerator.h'.
#include "ash/components/arc/mojom/video_encode_accelerator.mojom.h"

namespace mojo {

template <>
struct EnumTraits<arc::mojom::VideoFrameStorageType,
                  media::VideoEncodeAccelerator::Config::StorageType> {
  static arc::mojom::VideoFrameStorageType ToMojom(
      media::VideoEncodeAccelerator::Config::StorageType input);
  static bool FromMojom(
      arc::mojom::VideoFrameStorageType input,
      media::VideoEncodeAccelerator::Config::StorageType* output);
};

template <>
struct StructTraits<arc::mojom::VideoEncodeProfileDataView,
                    media::VideoEncodeAccelerator::SupportedProfile> {
  static media::VideoCodecProfile profile(
      const media::VideoEncodeAccelerator::SupportedProfile& r) {
    return r.profile;
  }
  static const gfx::Size& max_resolution(
      const media::VideoEncodeAccelerator::SupportedProfile& r) {
    return r.max_resolution;
  }
  static uint32_t max_framerate_numerator(
      const media::VideoEncodeAccelerator::SupportedProfile& r) {
    return r.max_framerate_numerator;
  }
  static uint32_t max_framerate_denominator(
      const media::VideoEncodeAccelerator::SupportedProfile& r) {
    return r.max_framerate_denominator;
  }

  static bool Read(arc::mojom::VideoEncodeProfileDataView data,
                   media::VideoEncodeAccelerator::SupportedProfile* out) {
    NOTIMPLEMENTED();
    return false;
  }
};

// TODO(b/198127993): Convert directly to media::Bitrate.
template <>
struct StructTraits<arc::mojom::ConstantBitrateDataView,
                    arc::mojom::ConstantBitrate> {
  static uint32_t target(const arc::mojom::ConstantBitrate& input);
};

template <>
struct StructTraits<arc::mojom::VariableBitrateDataView,
                    arc::mojom::VariableBitrate> {
  static uint32_t target(const arc::mojom::VariableBitrate& input);
  static uint32_t peak(const arc::mojom::VariableBitrate& input);
};

template <>
struct UnionTraits<arc::mojom::BitrateDataView, media::Bitrate> {
  static arc::mojom::BitrateDataView::Tag GetTag(const media::Bitrate& input);
  static arc::mojom::ConstantBitrate constant(const media::Bitrate& input);
  static arc::mojom::VariableBitrate variable(const media::Bitrate& input);
  static bool Read(arc::mojom::BitrateDataView input, media::Bitrate* output);
};

template <>
struct StructTraits<arc::mojom::VideoEncodeAcceleratorConfigDataView,
                    media::VideoEncodeAccelerator::Config> {
  static media::VideoPixelFormat input_format(
      const media::VideoEncodeAccelerator::Config& input) {
    return input.input_format;
  }

  static const gfx::Size& input_visible_size(
      const media::VideoEncodeAccelerator::Config& input) {
    return input.input_visible_size;
  }

  static media::VideoCodecProfile output_profile(
      const media::VideoEncodeAccelerator::Config& input) {
    return input.output_profile;
  }

  static uint32_t initial_bitrate_deprecated(
      const media::VideoEncodeAccelerator::Config& input) {
    return input.bitrate.target_bps();
  }

  static uint32_t initial_framerate(
      const media::VideoEncodeAccelerator::Config& input) {
    return input.framerate;
  }

  static uint32_t has_initial_framerate_deprecated(
      const media::VideoEncodeAccelerator::Config& input) {
    return true;
  }

  static uint32_t gop_length(
      const media::VideoEncodeAccelerator::Config& input) {
    return input.gop_length.value_or(0);
  }

  static bool has_gop_length(
      const media::VideoEncodeAccelerator::Config& input) {
    return input.gop_length.has_value();
  }

  static uint8_t h264_output_level(
      const media::VideoEncodeAccelerator::Config& input) {
    return input.h264_output_level.value_or(0);
  }

  static bool has_h264_output_level(
      const media::VideoEncodeAccelerator::Config& input) {
    return input.h264_output_level.has_value();
  }

  static arc::mojom::VideoFrameStorageType storage_type(
      const media::VideoEncodeAccelerator::Config& input) {
    switch (input.storage_type) {
      case media::VideoEncodeAccelerator::Config::StorageType::kShmem:
        return arc::mojom::VideoFrameStorageType::SHMEM;
      case media::VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer:
        return arc::mojom::VideoFrameStorageType::DMABUF;
    }
  }

  static const media::Bitrate& bitrate(
      const media::VideoEncodeAccelerator::Config& input) {
    return input.bitrate;
  }

  static bool Read(arc::mojom::VideoEncodeAcceleratorConfigDataView input,
                   media::VideoEncodeAccelerator::Config* output);
};

}  // namespace mojo

#endif  // ASH_COMPONENTS_ARC_MOJOM_VIDEO_ENCODE_ACCELERATOR_MOJOM_TRAITS_H_
