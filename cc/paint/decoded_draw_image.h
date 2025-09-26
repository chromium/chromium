// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_DECODED_DRAW_IMAGE_H_
#define CC_PAINT_DECODED_DRAW_IMAGE_H_

#include <cfloat>
#include <cmath>
#include <optional>

#include "cc/paint/paint_export.h"
#include "cc/paint/paint_flags.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSize.h"
#include "ui/gfx/hdr_metadata.h"

namespace cc {

class ColorFilter;

// A DecodedDrawImage is a finalized (decoded, scaled, colorspace converted,
// possibly uploaded) version of a DrawImage.  When this image is going to
// be serialized, it uses the transfer cache entry id (see the function
// PaintOpWriter::Write(DrawImage&) constructor.  When this image is going
// to be rastered directly, it uses the SkImage constructor.
class CC_PAINT_EXPORT DecodedDrawImage {
 public:
  DecodedDrawImage(sk_sp<SkImage> image,
                   sk_sp<ColorFilter> dark_mode_color_filter,
                   const SkSize& src_rect_offset,
                   const SkSize& scale_adjustment,
                   PaintFlags::FilterQuality filter_quality,
                   bool is_budgeted);
  DecodedDrawImage(sk_sp<SkImage> image,
                   sk_sp<SkImage> gainmap_image,
                   const std::optional<gfx::HDRMetadata>& hdr_metadata,
                   sk_sp<ColorFilter> dark_mode_color_filter,
                   const SkSize& src_rect_offset,
                   const SkSize& scale_adjustment,
                   PaintFlags::FilterQuality filter_quality,
                   bool is_budgeted);
  DecodedDrawImage(const gpu::Mailbox& mailbox,
                   PaintFlags::FilterQuality filter_quality);
  DecodedDrawImage(std::optional<uint32_t> transfer_cache_entry_id,
                   sk_sp<ColorFilter> dark_mode_color_filter,
                   const SkSize& src_rect_offset,
                   const SkSize& scale_adjustment,
                   PaintFlags::FilterQuality filter_quality,
                   bool needs_mips,
                   bool is_budgeted);
  DecodedDrawImage(const DecodedDrawImage& other);
  DecodedDrawImage(DecodedDrawImage&& other);
  DecodedDrawImage& operator=(const DecodedDrawImage&);
  DecodedDrawImage& operator=(DecodedDrawImage&&);

  DecodedDrawImage();
  ~DecodedDrawImage();

  const sk_sp<SkImage>& image() const { return image_; }
  const sk_sp<SkImage>& gainmap_image() const { return gainmap_image_; }
  const std::optional<gfx::HDRMetadata>& hdr_metadata() const {
    return hdr_metadata_;
  }
  const sk_sp<ColorFilter>& dark_mode_color_filter() const {
    return dark_mode_color_filter_;
  }
  std::optional<uint32_t> transfer_cache_entry_id() const {
    return transfer_cache_entry_id_;
  }

  // The source rect offset and scale adjustment are in the coordinate system
  // of `image()` and not `gainmap_image()`.
  const SkSize& src_rect_offset() const { return src_rect_offset_; }
  const SkSize& scale_adjustment() const { return scale_adjustment_; }
  PaintFlags::FilterQuality filter_quality() const { return filter_quality_; }
  bool is_scale_adjustment_identity() const {
    return std::abs(scale_adjustment_.width() - 1.f) < FLT_EPSILON &&
           std::abs(scale_adjustment_.height() - 1.f) < FLT_EPSILON;
  }
  bool transfer_cache_entry_needs_mips() const {
    return transfer_cache_entry_needs_mips_;
  }
  bool is_budgeted() const { return is_budgeted_; }
  const gpu::Mailbox& mailbox() const { return mailbox_; }
  explicit operator bool() const {
    return image_ || transfer_cache_entry_id_ || !mailbox_.IsZero();
  }

 private:
  sk_sp<SkImage> image_;
  sk_sp<SkImage> gainmap_image_;
  std::optional<gfx::HDRMetadata> hdr_metadata_;
  gpu::Mailbox mailbox_;
  std::optional<uint32_t> transfer_cache_entry_id_;
  sk_sp<ColorFilter> dark_mode_color_filter_;
  SkSize src_rect_offset_;
  SkSize scale_adjustment_;
  PaintFlags::FilterQuality filter_quality_;
  bool transfer_cache_entry_needs_mips_ = false;
  bool is_budgeted_;
};

}  // namespace cc

#endif  // CC_PAINT_DECODED_DRAW_IMAGE_H_
