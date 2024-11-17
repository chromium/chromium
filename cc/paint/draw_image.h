// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_DRAW_IMAGE_H_
#define CC_PAINT_DRAW_IMAGE_H_

#include <optional>

#include "cc/paint/paint_export.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_image.h"
#include "cc/paint/target_color_params.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkM44.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size_f.h"

namespace cc {

// A DrawImage is a logical snapshot in time and space of a PaintImage.  It
// includes decisions about scaling, animation frame, final colorspace, etc.
// It has not been decoded yet.  DrawImage turns into DecodedDrawImage via
// ImageDecodeCache::GetDecodedImageForDraw during playback.
class CC_PAINT_EXPORT DrawImage {
 public:
  DrawImage();
  explicit DrawImage(PaintImage image);
  DrawImage(PaintImage image,
            bool use_dark_mode,
            const SkIRect& src_rect,
            PaintFlags::FilterQuality filter_quality,
            const SkM44& matrix,
            std::optional<size_t> frame_index = std::nullopt);
  DrawImage(PaintImage image,
            bool use_dark_mode,
            const SkIRect& src_rect,
            PaintFlags::FilterQuality filter_quality,
            const SkM44& matrix,
            std::optional<size_t> frame_index,
            const TargetColorParams& target_color_params);
  // Constructs a DrawImage from |other| by adjusting its scale and setting new
  // color params.
  DrawImage(const DrawImage& other,
            float scale_adjustment,
            size_t frame_index,
            const TargetColorParams& target_color_params);
  DrawImage(const DrawImage& other);
  DrawImage(DrawImage&& other);
  ~DrawImage();

  DrawImage& operator=(DrawImage&& other);
  DrawImage& operator=(const DrawImage& other);

  // For testing only. Checks if `this` and `other` are the same image, i.e.
  // share the same underlying image. `a.IsSameForTesting(b)` will be true after
  // `DrawImage b = a;`.
  bool IsSameForTesting(const DrawImage& other) const;

  const PaintImage& paint_image() const { return paint_image_; }
  bool use_dark_mode() const { return use_dark_mode_; }
  const SkSize& scale() const { return scale_; }
  const SkIRect& src_rect() const { return src_rect_; }
  PaintFlags::FilterQuality filter_quality() const { return filter_quality_; }
  bool matrix_is_decomposable() const { return matrix_is_decomposable_; }
  PaintImage::FrameKey frame_key() const {
    return paint_image_.GetKeyForFrame(frame_index());
  }
  size_t frame_index() const {
    DCHECK(frame_index_.has_value());
    return frame_index_.value();
  }

  const TargetColorParams& target_color_params() const {
    DCHECK(target_color_params_.has_value());
    return *target_color_params_;
  }
  const gfx::ColorSpace& target_color_space() const {
    DCHECK(target_color_params_.has_value());
    return target_color_params_->color_space;
  }
  float sdr_white_level() const {
    DCHECK(target_color_params_.has_value());
    return target_color_params_->sdr_max_luminance_nits;
  }

 private:
  void SetTargetColorParams(const TargetColorParams& target_color_params);

  PaintImage paint_image_;
  bool use_dark_mode_;
  SkIRect src_rect_;
  PaintFlags::FilterQuality filter_quality_;
  SkSize scale_;
  bool matrix_is_decomposable_;
  std::optional<size_t> frame_index_;
  std::optional<TargetColorParams> target_color_params_;
};

}  // namespace cc

#endif  // CC_PAINT_DRAW_IMAGE_H_
