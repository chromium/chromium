// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/rounded_display/rounded_display_gutter.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "ash/frame_sink/ui_resource.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/geometry/vector2d.h"

namespace ash {
namespace {

using RoundedCornerPosition = RoundedDisplayGutter::RoundedCorner::Position;
using RoundedCorner = RoundedDisplayGutter::RoundedCorner;

}  // namespace

RoundedCorner::RoundedCorner(Position position,
                             int radius,
                             const gfx::Point& origin)
    : position_(position),
      radius_(radius),
      bounds_in_pixels_(gfx::Rect(origin, gfx::Size(radius, radius))) {}

RoundedCorner& RoundedCorner::operator=(RoundedCorner&& other) = default;
RoundedCorner::RoundedCorner(RoundedCorner&& other) = default;

RoundedCorner::~RoundedCorner() = default;

bool RoundedDisplayGutter::RoundedCorner::DoesPaint() const {
  return radius_ > 0;
}

void RoundedDisplayGutter::RoundedCorner::Paint(gfx::Canvas* canvas) const {
  if (!DoesPaint()) {
    return;
  }

  // To draw a rounded corner, we draw a black square of size 2 * radius and
  // subtract out a circle of radius r to get the corner shape.

  // We translate the canvas such that the top-left of the drawn square
  // is at the origin of the canvas. This simplifies the drawing logic.
  const int translate_dx = (position_ == RoundedCornerPosition::kUpperRight ||
                            position_ == RoundedCornerPosition::kLowerRight)
                               ? -radius_
                               : 0;
  const int translate_dy = (position_ == RoundedCornerPosition::kLowerLeft ||
                            position_ == RoundedCornerPosition::kLowerRight)
                               ? -radius_
                               : 0;

  canvas->Save();
  canvas->Translate({translate_dx, translate_dy});

  // We clip the canvas to the region that should contain the corner. This
  // ensures we only draw the one corner we want and don't draw the other three
  // corners of the rounded rect.
  canvas->ClipRect(gfx::Rect(-translate_dx, -translate_dy, radius_, radius_));

  gfx::Rect square_rect(0, 0, 2 * radius_, 2 * radius_);
  SkRRect r_rect =
      SkRRect::MakeRectXY(gfx::RectToSkRect(square_rect), radius_, radius_);
  canvas->sk_canvas()->clipRRect(r_rect, SkClipOp::kDifference, true);
  canvas->FillRect(square_rect, SK_ColorBLACK);

  canvas->Restore();
}

// -----------------------------------------------------------------------------
// RoundedDisplayGutter:

// static
std::unique_ptr<RoundedDisplayGutter> RoundedDisplayGutter::CreateGutter(
    std::vector<RoundedCorner>&& corners,
    bool is_overlay) {
  return std::make_unique<RoundedDisplayGutter>(std::move(corners), is_overlay);
}

RoundedDisplayGutter::RoundedDisplayGutter(std::vector<RoundedCorner>&& corners,
                                           bool is_overlay)
    : corners_(std::move(corners)), is_overlay_(is_overlay) {
  // A gutter must paint at least one rounded corner and at most four corners.
  DCHECK(corners_.size() > 0 && corners_.size() <= 4);

  // Since the corners of the gutter cannot be changed, both gutter bounds and
  // ui_source_id do not change either.
  bounds_in_pixels_ = CalculateGutterBounds();
  ui_source_id_ = CalculateUiSourceId();
  DCHECK(ui_source_id_ != kInvalidUiSourceId);
}

RoundedDisplayGutter::~RoundedDisplayGutter() = default;

UiSourceId RoundedDisplayGutter::ui_source_id() const {
  return ui_source_id_;
}

UiSourceId RoundedDisplayGutter::CalculateUiSourceId() const {
  UiSourceId ui_source_id = kInvalidUiSourceId;
  // Value of the position mask of the gutter will give a unique value for any
  // combination of RoundedDisplayCorners.
  for (const auto& corner : corners_) {
    ui_source_id |= corner.position();
  }

  return ui_source_id;
}

gfx::Rect RoundedDisplayGutter::CalculateGutterBounds() const {
  gfx::Rect gutter_bounds;

  for (const auto& corner : corners_) {
    gutter_bounds.Union(corner.bounds());
  }

  return gutter_bounds;
}

const gfx::Rect& RoundedDisplayGutter::bounds() const {
  return bounds_in_pixels_;
}

void RoundedDisplayGutter::Paint(gfx::Canvas* canvas) const {
  for (const auto& corner : corners_) {
    canvas->Save();
    const gfx::Vector2d offset =
        corner.bounds().OffsetFromOrigin() - bounds().OffsetFromOrigin();
    canvas->Translate(offset);
    corner.Paint(canvas);
    canvas->Restore();
  }
}

}  // namespace ash
