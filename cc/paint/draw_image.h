// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_DRAW_IMAGE_H_
#define CC_PAINT_DRAW_IMAGE_H_

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

// A DrawImage is a logical snapshot in time and space of a PaintImage, without
// raster-target decisions (such as animation frame or target color space).
// During raster or image decode, it is combined with target parameters into a
// DrawImageWithTarget.
class CC_PAINT_EXPORT DrawImage {
 public:
  DrawImage();
  explicit DrawImage(PaintImage image);
  DrawImage(PaintImage image,
            bool use_dark_mode,
            const SkIRect& src_rect,
            PaintFlags::FilterQuality filter_quality,
            const SkM44& matrix);
  // Constructs a DrawImage from |other| by adjusting its scale.
  DrawImage(const DrawImage& other, float scale_adjustment);
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

 private:
  PaintImage paint_image_;
  bool use_dark_mode_;
  SkIRect src_rect_;
  PaintFlags::FilterQuality filter_quality_;
  SkSize scale_;
  bool matrix_is_decomposable_;
};

// A DrawImageWithTarget pairs a DrawImage with raster-time decisions about
// scaling, animation frame, and TargetColorParams. It has not been decoded yet.
// DrawImageWithTarget turns into DecodedDrawImage via
// ImageDecodeCache::GetDecodedImageForDraw during playback.
class CC_PAINT_EXPORT DrawImageWithTarget {
 public:
  DrawImageWithTarget();
  DrawImageWithTarget(PaintImage image,
                      bool use_dark_mode,
                      const SkIRect& src_rect,
                      PaintFlags::FilterQuality filter_quality,
                      const SkM44& matrix,
                      size_t frame_index,
                      const TargetColorParams& target_color_params);
  // Constructs a DrawImageWithTarget from |other| by adjusting its scale and
  // setting new frame index and color params.
  DrawImageWithTarget(const DrawImage& other,
                      float scale_adjustment,
                      size_t frame_index,
                      const TargetColorParams& target_color_params);
  DrawImageWithTarget(const DrawImageWithTarget& other);
  DrawImageWithTarget(DrawImageWithTarget&& other);
  ~DrawImageWithTarget();

  DrawImageWithTarget& operator=(DrawImageWithTarget&& other);
  DrawImageWithTarget& operator=(const DrawImageWithTarget& other);

  // For testing only. Checks if `this` and `other` are the same image, i.e.
  // share the same underlying image and target color params.
  bool IsSameForTesting(const DrawImageWithTarget& other) const;

  const DrawImage& draw_image() const { return draw_image_; }

  const PaintImage& paint_image() const { return draw_image_.paint_image(); }
  bool use_dark_mode() const { return draw_image_.use_dark_mode(); }
  const SkSize& scale() const { return draw_image_.scale(); }
  const SkIRect& src_rect() const { return draw_image_.src_rect(); }
  PaintFlags::FilterQuality filter_quality() const {
    return draw_image_.filter_quality();
  }
  bool matrix_is_decomposable() const {
    return draw_image_.matrix_is_decomposable();
  }
  PaintImage::FrameKey frame_key() const {
    return paint_image().GetKeyForFrame(frame_index());
  }
  size_t frame_index() const { return frame_index_; }

  const TargetColorParams& target_color_params() const {
    return target_color_params_;
  }
  const gfx::ColorSpace& target_color_space() const {
    return target_color_params_.color_space;
  }

 private:
  void SetTargetColorParams(const TargetColorParams& target_color_params);

  DrawImage draw_image_;
  size_t frame_index_ = PaintImage::kDefaultFrameIndex;
  TargetColorParams target_color_params_;
};

}  // namespace cc

#endif  // CC_PAINT_DRAW_IMAGE_H_
