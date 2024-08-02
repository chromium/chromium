// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/public/cpp/rounded_image_view.h"

#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace ash {

RoundedImageView::RoundedImageView()
    : RoundedImageView(/*corner_radius=*/0, Alignment::kLeading) {}

RoundedImageView::RoundedImageView(int corner_radius, Alignment alignment)
    : alignment_(alignment) {
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

void RoundedImageView::SetCornerRadius(int corner_radius) {
  SetCornerRadii(corner_radius, corner_radius, corner_radius, corner_radius);
}

gfx::Size RoundedImageView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(GetImageSize().width() + GetInsets().width(),
                   GetImageSize().height() + GetInsets().height());
}

void RoundedImageView::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);
  gfx::Rect drawn_image_bounds(size());
  drawn_image_bounds.Inset(GetInsets());

  // It handles the situation that the size of the drawing space is greater
  // than that of the image to draw.
  drawn_image_bounds.ClampToCenteredSize(GetImageSize());

  const SkScalar kRadius[8] = {
      SkIntToScalar(corner_radius_[0]), SkIntToScalar(corner_radius_[0]),
      SkIntToScalar(corner_radius_[1]), SkIntToScalar(corner_radius_[1]),
      SkIntToScalar(corner_radius_[2]), SkIntToScalar(corner_radius_[2]),
      SkIntToScalar(corner_radius_[3]), SkIntToScalar(corner_radius_[3])};
  SkPath path;
  path.addRoundRect(gfx::RectToSkRect(drawn_image_bounds), kRadius);
  cc::PaintFlags flags;
  flags.setAntiAlias(true);

  gfx::ImageSkia image_to_draw;
  switch (alignment_) {
    case Alignment::kLeading:
      image_to_draw = resized_image_;
      break;
    case Alignment::kCenter:
      gfx::Rect image_size(GetImageSize());

      // It handles the situation that the size of the image to draw is greater
      // than that of the drawing space.
      image_size.ClampToCenteredSize(drawn_image_bounds.size());

      image_to_draw =
          gfx::ImageSkiaOperations::ExtractSubset(resized_image_, image_size);
      break;
  }

  // The size of the area to paint `image_to_draw` should be no greater than
  // that of `image_to_draw`. Otherwise, `image_to_draw` will be tiled.
  DCHECK_LE(drawn_image_bounds.width(), image_to_draw.width());
  DCHECK_LE(drawn_image_bounds.height(), image_to_draw.height());

  canvas->DrawImageInPath(image_to_draw, drawn_image_bounds.x(),
                          drawn_image_bounds.y(), path, flags);
}

gfx::Size RoundedImageView::GetImageSize() const {
  return resized_image_.size();
}

BEGIN_METADATA(RoundedImageView)
END_METADATA

}  // namespace ash
