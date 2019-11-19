// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/draw_image.h"

namespace cc {
namespace {

// Helper funciton to extract a scale from the matrix. Returns true on success
// and false on failure.
bool ExtractScale(const SkMatrix& matrix, SkSize* scale) {
  *scale = SkSize::Make(matrix.getScaleX(), matrix.getScaleY());
  if (matrix.getType() & SkMatrix::kAffine_Mask) {
    if (!matrix.decomposeScale(scale)) {
      scale->set(1, 1);
      return false;
    }
  }
  return true;
}

}  // namespace

DrawImage::DrawImage()
    : src_rect_(SkIRect::MakeXYWH(0, 0, 0, 0)),
      filter_quality_(kNone_SkFilterQuality),
      scale_(SkSize::Make(1.f, 1.f)),
      matrix_is_decomposable_(true) {}

DrawImage::DrawImage(PaintImage image)
    : paint_image_(std::move(image)),
      src_rect_(
          SkIRect::MakeXYWH(0, 0, paint_image_.width(), paint_image_.height())),
      filter_quality_(kNone_SkFilterQuality),
      scale_(SkSize::Make(1.f, 1.f)),
      matrix_is_decomposable_(true) {}

DrawImage::DrawImage(PaintImage image,
                     const SkIRect& src_rect,
                     SkFilterQuality filter_quality,
                     const SkMatrix& matrix,
                     base::Optional<size_t> frame_index,
                     const base::Optional<gfx::ColorSpace>& color_space)
    : paint_image_(std::move(image)),
      src_rect_(src_rect),
      filter_quality_(filter_quality),
      frame_index_(frame_index),
      target_color_space_(color_space) {
  matrix_is_decomposable_ = ExtractScale(matrix, &scale_);
}

DrawImage::DrawImage(const DrawImage& other,
                     float scale_adjustment,
                     size_t frame_index,
                     const gfx::ColorSpace& color_space)
    : paint_image_(other.paint_image_),
      src_rect_(other.src_rect_),
      filter_quality_(other.filter_quality_),
      scale_(SkSize::Make(other.scale_.width() * scale_adjustment,
                          other.scale_.height() * scale_adjustment)),
      matrix_is_decomposable_(other.matrix_is_decomposable_),
      frame_index_(frame_index),
      target_color_space_(color_space) {}

DrawImage::DrawImage(const DrawImage& other) = default;
DrawImage::DrawImage(DrawImage&& other) = default;
DrawImage::~DrawImage() = default;

DrawImage& DrawImage::operator=(DrawImage&& other) = default;
DrawImage& DrawImage::operator=(const DrawImage& other) = default;

bool DrawImage::operator==(const DrawImage& other) const {
  return paint_image_ == other.paint_image_ && src_rect_ == other.src_rect_ &&
         filter_quality_ == other.filter_quality_ && scale_ == other.scale_ &&
         matrix_is_decomposable_ == other.matrix_is_decomposable_ &&
         target_color_space_ == other.target_color_space_;
}

}  // namespace cc
