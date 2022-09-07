// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_MOJOM_VIDEO_ACCELERATOR_MOJOM_TRAITS_H_
#define ASH_COMPONENTS_ARC_MOJOM_VIDEO_ACCELERATOR_MOJOM_TRAITS_H_

#include <memory>

#include "ash/components/arc/mojom/video_common.mojom-shared.h"
#include "ash/components/arc/mojom/video_decoder.mojom-shared.h"
#include "ash/components/arc/video_accelerator/video_frame_plane.h"
#include "media/base/color_plane_layout.h"
#include "media/base/decoder_status.h"
#include "media/base/status.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame_layout.h"
#include "media/base/video_types.h"
#include "ui/gfx/geometry/size.h"

namespace mojo {

template <>
struct EnumTraits<arc::mojom::VideoCodecProfile, media::VideoCodecProfile> {
  static arc::mojom::VideoCodecProfile ToMojom(media::VideoCodecProfile input);

  static bool FromMojom(arc::mojom::VideoCodecProfile input,
                        media::VideoCodecProfile* output);
};

template <>
struct StructTraits<arc::mojom::VideoFramePlaneDataView, arc::VideoFramePlane> {
  static int32_t offset(const arc::VideoFramePlane& r) {
    DCHECK_GE(r.offset, 0);
    return r.offset;
  }

  static int32_t stride(const arc::VideoFramePlane& r) {
    DCHECK_GE(r.stride, 0);
    return r.stride;
  }

  static bool Read(arc::mojom::VideoFramePlaneDataView data,
                   arc::VideoFramePlane* out);
};

template <>
struct StructTraits<arc::mojom::SizeDataView, gfx::Size> {
  static int width(const gfx::Size& r) {
    DCHECK_GE(r.width(), 0);
    return r.width();
  }

  static int height(const gfx::Size& r) {
    DCHECK_GE(r.height(), 0);
    return r.height();
  }

  static bool Read(arc::mojom::SizeDataView data, gfx::Size* out);
};

template <>
struct EnumTraits<arc::mojom::VideoPixelFormat, media::VideoPixelFormat> {
  static arc::mojom::VideoPixelFormat ToMojom(media::VideoPixelFormat input);

  static bool FromMojom(arc::mojom::VideoPixelFormat input,
                        media::VideoPixelFormat* output);
};

template <>
struct StructTraits<arc::mojom::ColorPlaneLayoutDataView,
                    media::ColorPlaneLayout> {
  static int32_t stride(const media::ColorPlaneLayout& r) { return r.stride; }

  static uint32_t offset(const media::ColorPlaneLayout& r) { return r.offset; }

  static uint32_t size(const media::ColorPlaneLayout& r) { return r.size; }

  static bool Read(arc::mojom::ColorPlaneLayoutDataView data,
                   media::ColorPlaneLayout* out);
};

// Because `media::VideoFrameLayout` doesn't have default constructor, we cannot
// convert from mojo struct directly. Instead, we map to the type
// `std::unique_ptr<media::VideoFrameLayout>`.
template <>
struct StructTraits<arc::mojom::VideoFrameLayoutDataView,
                    std::unique_ptr<media::VideoFrameLayout>> {
  static bool IsNull(const std::unique_ptr<media::VideoFrameLayout>& input) {
    return input.get() == nullptr;
  }

  static void SetToNull(std::unique_ptr<media::VideoFrameLayout>* output) {
    output->reset();
  }

  static media::VideoPixelFormat format(
      const std::unique_ptr<media::VideoFrameLayout>& input) {
    DCHECK(input);
    return input->format();
  }

  static const gfx::Size& coded_size(
      const std::unique_ptr<media::VideoFrameLayout>& input) {
    DCHECK(input);
    return input->coded_size();
  }

  static const std::vector<media::ColorPlaneLayout>& planes(
      const std::unique_ptr<media::VideoFrameLayout>& input) {
    DCHECK(input);
    return input->planes();
  }

  static bool is_multi_planar(
      const std::unique_ptr<media::VideoFrameLayout>& input) {
    DCHECK(input);
    return input->is_multi_planar();
  }

  static uint32_t buffer_addr_align(
      const std::unique_ptr<media::VideoFrameLayout>& input) {
    DCHECK(input);
    return input->buffer_addr_align();
  }

  static uint64_t modifier(
      const std::unique_ptr<media::VideoFrameLayout>& input) {
    DCHECK(input);
    return input->modifier();
  }

  static bool Read(arc::mojom::VideoFrameLayoutDataView data,
                   std::unique_ptr<media::VideoFrameLayout>* out);
};

template <>
struct EnumTraits<arc::mojom::DecoderStatus, media::DecoderStatus> {
  static arc::mojom::DecoderStatus ToMojom(media::DecoderStatus input);

  static bool FromMojom(arc::mojom::DecoderStatus input,
                        media::DecoderStatus* output);
};

}  // namespace mojo

#endif  // ASH_COMPONENTS_ARC_MOJOM_VIDEO_ACCELERATOR_MOJOM_TRAITS_H_
