// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/rounded_image_view.h"

#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/skia_util.h"

namespace ash {

RoundedImageView::RoundedImageView(int corner_radius) {
  for (int i = 0; i < 4; ++i)
    corner_radius_[i] = corner_radius;
}

RoundedImageView::~RoundedImageView() = default;

void RoundedImageView::SetImage(const gfx::ImageSkia& image) {
  SetImage(image, image.size());
}

void RoundedImageView::SetImage(const gfx::ImageSkia& image,
                                const gfx::Size& size) {
  const bool is_size_same = GetImageSize() == size;
  const bool is_image_same = original_image_.BackedBySameObjectAs(image);
  if (is_size_same && is_image_same)
    return;

  if (!is_image_same)
    original_image_ = image;

  // Try to get the best image quality for the avatar.
  resized_image_ = gfx::ImageSkiaOperations::CreateResizedImage(
      original_image_, skia::ImageOperations::RESIZE_BEST, size);

  if (GetWidget() && GetVisible()) {
    PreferredSizeChanged();
    SchedulePaint();
  }
}

void RoundedImageView::SetCornerRadii(int top_left,
                                      int top_right,
                                      int bottom_right,
                                      int bottom_left) {
  corner_radius_[0] = top_left;
  corner_radius_[1] = top_right;
  corner_radius_[2] = bottom_right;
  corner_radius_[3] = bottom_left;
}

gfx::Size RoundedImageView::CalculatePreferredSize() const {
  return gfx::Size(GetImageSize().width() + GetInsets().width(),
                   GetImageSize().height() + GetInsets().height());
}

void RoundedImageView::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);
  gfx::Rect image_bounds(size());
  image_bounds.ClampToCenteredSize(GetPreferredSize());
  image_bounds.Inset(GetInsets());
  const SkScalar kRadius[8] = {
      SkIntToScalar(corner_radius_[0]), SkIntToScalar(corner_radius_[0]),
      SkIntToScalar(corner_radius_[1]), SkIntToScalar(corner_radius_[1]),
      SkIntToScalar(corner_radius_[2]), SkIntToScalar(corner_radius_[2]),
      SkIntToScalar(corner_radius_[3]), SkIntToScalar(corner_radius_[3])};
  SkPath path;
  path.addRoundRect(gfx::RectToSkRect(image_bounds), kRadius);
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  canvas->DrawImageInPath(resized_image_, image_bounds.x(), image_bounds.y(),
                          path, flags);
}

const char* RoundedImageView::GetClassName() const {
  return "RoundedImageView";
}

gfx::Size RoundedImageView::GetImageSize() const {
  return resized_image_.size();
}

}  // namespace ash
