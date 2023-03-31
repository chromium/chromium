// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/rounded_display/rounded_display_gutter.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "ash/frame_sink/ui_resource.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
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

  PaintCornerHelper(canvas);
}

void RoundedDisplayGutter::RoundedCorner::PaintCornerHelper(
    gfx::Canvas* canvas) const {
  SkPath path;
  SkScalar startAngle = 0.0, sweepAngle = 0.0;
  SkScalar dx = 0.0, dy = 0.0;
  int translate_dx = 0.0, translate_dy = 0.0;

  switch (position_) {
    case RoundedCornerPosition::kUpperLeft:
      startAngle = -90;
      sweepAngle = -90;
      dx = radius_;
      dy = -radius_;
      translate_dx = 0;
      translate_dy = 0;
      break;
    case RoundedCornerPosition::kLowerLeft:
      startAngle = 90;
      sweepAngle = 90;
      dx = radius_;
      dy = radius_;
      translate_dx = 0;
      translate_dy = -radius_;
      break;
    case RoundedCornerPosition::kUpperRight:
      startAngle = 0;
      sweepAngle = -90;
      dx = radius_;
      dy = radius_;
      translate_dx = -radius_;
      translate_dy = 0;
      break;
    case RoundedCornerPosition::kLowerRight:
      startAngle = 0;
      sweepAngle = 90;
      dx = radius_;
      dy = -radius_;
      translate_dx = -radius_;
      translate_dy = -radius_;
      break;
  }

  const SkScalar oval_radius = radius_ * 2;
  SkRect oval{0, 0, oval_radius, oval_radius};

  path.addArc(oval, startAngle, sweepAngle);

  if (position_ == RoundedCornerPosition::kUpperLeft ||
      position_ == RoundedCornerPosition::kLowerLeft) {
    path.rLineTo(0, dy);
    path.rLineTo(dx, 0);
  }

  if (position_ == RoundedCornerPosition::kUpperRight ||
      position_ == RoundedCornerPosition::kLowerRight) {
    path.rLineTo(dx, 0);
    path.rLineTo(0, dy);
  }

  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::Style::kFill_Style);
  flags.setAntiAlias(true);
  flags.setColor(SK_ColorBLACK);

  canvas->Save();
  canvas->Translate({translate_dx, translate_dy});
  canvas->DrawPath(path, flags);
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
