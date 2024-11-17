// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/draw_image.h"

#include <utility>

namespace cc {
namespace {

// Helper funciton to extract a scale from the matrix. Returns true on success
// and false on failure.
bool ExtractScale(const SkM44& matrix, SkSize* scale) {
  *scale = SkSize::Make(matrix.rc(0, 0), matrix.rc(1, 1));
  // TODO(crbug.com/40735471): Don't use SkMatrix here, add functionality to
  // MathUtil.
  SkMatrix mat33 = matrix.asM33();
  if (mat33.getType() & SkMatrix::kAffine_Mask) {
    if (!mat33.decomposeScale(scale)) {
      scale->set(1, 1);
      return false;
    }
  }
  return true;
}

}  // namespace

DrawImage::DrawImage()
    : use_dark_mode_(false),
      src_rect_(SkIRect::MakeXYWH(0, 0, 0, 0)),
      filter_quality_(PaintFlags::FilterQuality::kNone),
      scale_(SkSize::Make(1.f, 1.f)),
      matrix_is_decomposable_(true) {}

DrawImage::DrawImage(PaintImage image)
    : paint_image_(std::move(image)),
      use_dark_mode_(false),
      src_rect_(
          SkIRect::MakeXYWH(0, 0, paint_image_.width(), paint_image_.height())),
      filter_quality_(PaintFlags::FilterQuality::kNone),
      scale_(SkSize::Make(1.f, 1.f)),
      matrix_is_decomposable_(true) {}

DrawImage::DrawImage(PaintImage image,
                     bool use_dark_mode,
                     const SkIRect& src_rect,
                     PaintFlags::FilterQuality filter_quality,
                     const SkM44& matrix,
                     std::optional<size_t> frame_index)
    : paint_image_(std::move(image)),
      use_dark_mode_(use_dark_mode),
      src_rect_(src_rect),
      filter_quality_(filter_quality),
      frame_index_(frame_index) {
  matrix_is_decomposable_ = ExtractScale(matrix, &scale_);
}

DrawImage::DrawImage(PaintImage image,
                     bool use_dark_mode,
                     const SkIRect& src_rect,
                     PaintFlags::FilterQuality filter_quality,
                     const SkM44& matrix,
                     std::optional<size_t> frame_index,
                     const TargetColorParams& target_color_params)
    : paint_image_(std::move(image)),
      use_dark_mode_(use_dark_mode),
      src_rect_(src_rect),
      filter_quality_(filter_quality),
      frame_index_(frame_index) {
  SetTargetColorParams(target_color_params);
  matrix_is_decomposable_ = ExtractScale(matrix, &scale_);
}

DrawImage::DrawImage(const DrawImage& other,
                     float scale_adjustment,
                     size_t frame_index,
                     const TargetColorParams& target_color_params)
    : paint_image_(other.paint_image_),
      use_dark_mode_(other.use_dark_mode_),
      src_rect_(other.src_rect_),
      filter_quality_(other.filter_quality_),
      scale_(SkSize::Make(other.scale_.width() * scale_adjustment,
                          other.scale_.height() * scale_adjustment)),
      matrix_is_decomposable_(other.matrix_is_decomposable_),
      frame_index_(frame_index),
      target_color_params_(target_color_params) {
  SetTargetColorParams(target_color_params);
}

DrawImage::DrawImage(const DrawImage& other) = default;
DrawImage::DrawImage(DrawImage&& other) = default;
DrawImage::~DrawImage() = default;

DrawImage& DrawImage::operator=(DrawImage&& other) = default;
DrawImage& DrawImage::operator=(const DrawImage& other) = default;

bool DrawImage::IsSameForTesting(const DrawImage& other) const {
  return paint_image_.IsSameForTesting(other.paint_image_) &&  // IN-TEST
         use_dark_mode_ == other.use_dark_mode_ &&
         src_rect_ == other.src_rect_ &&
         filter_quality_ == other.filter_quality_ && scale_ == other.scale_ &&
         matrix_is_decomposable_ == other.matrix_is_decomposable_ &&
         target_color_params_ == other.target_color_params_;
}

void DrawImage::SetTargetColorParams(
    const TargetColorParams& target_color_params) {
  target_color_params_ = target_color_params;
  if (paint_image_.GetReinterpretAsSRGB()) {
    // If `paint_image_` is to be reinterpreted as sRGB, then the target color
    // space (used by decode) is invalid (which indicates that the color profile
    // is to be ignored).
    target_color_params_->color_space = gfx::ColorSpace();
  }
}

}  // namespace cc
