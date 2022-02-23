// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/highlight_border.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/style/scoped_light_mode_as_default.h"
#include "ash/style/ash_color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/views/view.h"

namespace ash {

constexpr int kHighlightBorderThickness = 1;

HighlightBorder::HighlightBorder(int corner_radius,
                                 Type type,
                                 bool use_light_colors)
    : corner_radius_(corner_radius),
      type_(type),
      use_light_colors_(use_light_colors) {}

void HighlightBorder::Paint(const views::View& view, gfx::Canvas* canvas) {
  AshColorProvider* color_provider = AshColorProvider::Get();
  const AshColorProvider::ControlsLayerType highlight_color_type =
      type_ == HighlightBorder::Type::kHighlightBorder1
          ? AshColorProvider::ControlsLayerType::kHighlightColor1
          : AshColorProvider::ControlsLayerType::kHighlightColor2;
  const AshColorProvider::ControlsLayerType border_color_type =
      type_ == HighlightBorder::Type::kHighlightBorder1
          ? AshColorProvider::ControlsLayerType::kBorderColor1
          : AshColorProvider::ControlsLayerType::kBorderColor2;
  SkColor inner_color =
      color_provider->GetControlsLayerColor(highlight_color_type);
  SkColor outer_color =
      color_provider->GetControlsLayerColor(border_color_type);

  if (use_light_colors_ && !features::IsDarkLightModeEnabled()) {
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
  gfx::RectF outer_border_bounds(view.GetLocalBounds());
  outer_border_bounds.Inset(half_thickness, half_thickness);
  canvas->DrawRoundRect(outer_border_bounds, corner_radius_, flags);

  gfx::RectF inner_border_bounds(view.GetLocalBounds());
  inner_border_bounds.Inset(gfx::Insets(kHighlightBorderThickness));
  inner_border_bounds.Inset(half_thickness, half_thickness);
  flags.setColor(inner_color);
  canvas->DrawRoundRect(inner_border_bounds, corner_radius_, flags);
}

gfx::Insets HighlightBorder::GetInsets() const {
  return gfx::Insets(2 * kHighlightBorderThickness);
}

gfx::Size HighlightBorder::GetMinimumSize() const {
  return gfx::Size(kHighlightBorderThickness * 4,
                   kHighlightBorderThickness * 4);
}

}  // namespace ash