// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/gestures/back_gesture/back_gesture_util.h"

#include "ash/style/ash_color_provider.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/highlight_border.h"

namespace ash {

namespace {

constexpr float kOuterHightlightBorderThickness =
    1.5 * views::kHighlightBorderThickness;
constexpr float kInnerHightlightBorderThickness =
    views::kHighlightBorderThickness / 2.f;

SkColor GetHighlightBorderInnerColor() {
  return AshColorProvider::Get()->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kHighlightColor1);
}

SkColor GetHighlightBorderOuterColor() {
  return AshColorProvider::Get()->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kBorderColor1);
}

cc::PaintFlags GetHighlightBorderPaintFlags() {
  cc::PaintFlags hb_flags;
  hb_flags.setStrokeWidth(views::kHighlightBorderThickness);
  hb_flags.setStyle(cc::PaintFlags::kStroke_Style);
  hb_flags.setAntiAlias(true);
  return hb_flags;
}

}  // namespace

void DrawCircleHighlightBorder(gfx::Canvas* canvas,
                               const gfx::PointF& circle_center,
                               int radius) {
  gfx::ScopedCanvas scoped_canvas(canvas);
  // Scale bounds and corner radius with device scale factor to make sure
  // border bounds match content bounds but keep border stroke width the same.
  const float dsf = canvas->UndoDeviceScaleFactor();
  const float scaled_corner_radius = dsf * radius;
  gfx::PointF scaled_circle_center = circle_center;
  scaled_circle_center.Scale(dsf);
  cc::PaintFlags hb_flags = GetHighlightBorderPaintFlags();

  hb_flags.setColor(GetHighlightBorderOuterColor());
  canvas->DrawCircle(scaled_circle_center,
                     scaled_corner_radius + kOuterHightlightBorderThickness,
                     hb_flags);
  hb_flags.setColor(GetHighlightBorderInnerColor());
  canvas->DrawCircle(scaled_circle_center,
                     scaled_corner_radius + kInnerHightlightBorderThickness,
                     hb_flags);
}

void DrawRoundRectHighlightBorder(gfx::Canvas* canvas,
                                  const gfx::Rect& bounds,
                                  int corner_radius) {
  gfx::ScopedCanvas scoped_canvas(canvas);
  const float dsf = canvas->UndoDeviceScaleFactor();
  const float scaled_corner_radius = dsf * corner_radius;
  auto scaled_outer_bounds = gfx::ScaleToEnclosingRect(bounds, dsf);
  scaled_outer_bounds.Inset(-kOuterHightlightBorderThickness);
  auto scaled_inner_bounds = gfx::ScaleToEnclosingRect(bounds, dsf);
  scaled_inner_bounds.Inset(-kInnerHightlightBorderThickness);

  cc::PaintFlags hb_flags = GetHighlightBorderPaintFlags();

  hb_flags.setColor(GetHighlightBorderOuterColor());
  canvas->DrawRoundRect(scaled_outer_bounds,
                        scaled_corner_radius + kOuterHightlightBorderThickness,
                        hb_flags);
  hb_flags.setColor(GetHighlightBorderInnerColor());
  canvas->DrawRoundRect(scaled_inner_bounds,
                        scaled_corner_radius + kInnerHightlightBorderThickness,
                        hb_flags);
}

}  // namespace ash