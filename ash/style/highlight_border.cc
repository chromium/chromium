// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/highlight_border.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/style/scoped_light_mode_as_default.h"
#include "ash/style/ash_color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/view.h"

namespace ash {

constexpr int kHighlightBorderThickness = 1;

// static
void HighlightBorder::PaintBorderToCanvas(gfx::Canvas* canvas,
                                          const gfx::Rect& bounds,
                                          int corner_radius,
                                          HighlightBorder::Type type,
                                          bool use_light_colors) {
  AshColorProvider* color_provider = AshColorProvider::Get();
  const AshColorProvider::ControlsLayerType highlight_color_type =
      type == HighlightBorder::Type::kHighlightBorder1
          ? AshColorProvider::ControlsLayerType::kHighlightColor1
          : AshColorProvider::ControlsLayerType::kHighlightColor2;
  const AshColorProvider::ControlsLayerType border_color_type =
      type == HighlightBorder::Type::kHighlightBorder1
          ? AshColorProvider::ControlsLayerType::kBorderColor1
          : AshColorProvider::ControlsLayerType::kBorderColor2;
  SkColor inner_color =
      color_provider->GetControlsLayerColor(highlight_color_type);
  SkColor outer_color =
      color_provider->GetControlsLayerColor(border_color_type);

  if (use_light_colors && !features::IsDarkLightModeEnabled()) {
    ScopedLightModeAsDefault scoped_light_mode_as_default;
    inner_color = color_provider->GetControlsLayerColor(highlight_color_type);
    outer_color = color_provider->GetControlsLayerColor(border_color_type);
  }

  cc::PaintFlags flags;
  flags.setStrokeWidth(kHighlightBorderThickness);
  flags.setColor(outer_color);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setAntiAlias(true);

  const float half_thickness = kHighlightBorderThickness / 2.0f;

  // Scale bounds and corner radius with device scale factor to make sure
  // border bounds match content bounds but keep border stroke width the same.
  gfx::ScopedCanvas scoped_canvas(canvas);
  const float dsf = canvas->UndoDeviceScaleFactor();
  const gfx::RectF pixel_bounds = gfx::ConvertRectToPixels(bounds, dsf);
  const float scaled_corner_radius = dsf * corner_radius;
  gfx::RectF outer_border_bounds(pixel_bounds);

  outer_border_bounds.Inset(half_thickness, half_thickness);
  canvas->DrawRoundRect(outer_border_bounds, scaled_corner_radius, flags);

  gfx::RectF inner_border_bounds(pixel_bounds);
  inner_border_bounds.Inset(gfx::Insets(kHighlightBorderThickness));
  inner_border_bounds.Inset(half_thickness, half_thickness);
  flags.setColor(inner_color);
  canvas->DrawRoundRect(inner_border_bounds, scaled_corner_radius, flags);
}

HighlightBorder::HighlightBorder(int corner_radius,
                                 Type type,
                                 bool use_light_colors,
                                 InsetsType insets_type)
    : corner_radius_(corner_radius),
      type_(type),
      use_light_colors_(use_light_colors),
      insets_type_(insets_type) {}

void HighlightBorder::Paint(const views::View& view, gfx::Canvas* canvas) {
  PaintBorderToCanvas(canvas, view.GetLocalBounds(), corner_radius_, type_,
                      use_light_colors_);
}

gfx::Insets HighlightBorder::GetInsets() const {
  switch (insets_type_) {
    case InsetsType::kNoInsets:
      return gfx::Insets();
    case InsetsType::kHalfInsets:
      return gfx::Insets(kHighlightBorderThickness);
    case InsetsType::kFullInsets:
      return gfx::Insets(2 * kHighlightBorderThickness);
  }
}

gfx::Size HighlightBorder::GetMinimumSize() const {
  return gfx::Size(kHighlightBorderThickness * 4,
                   kHighlightBorderThickness * 4);
}

}  // namespace ash