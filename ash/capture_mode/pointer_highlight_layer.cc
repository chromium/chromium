// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/pointer_highlight_layer.h"

#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/capture_mode/capture_mode_util.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "base/check.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/highlight_border.h"

namespace ash {

namespace {

constexpr float kLightModeBorderOpacityScaleFactor = 0.8f;
const int kHighlightStrokeWidth = 2;
constexpr int kFillsRadius =
    capture_mode::kHighlightLayerRadius - kHighlightStrokeWidth;

int CalculateRadiusWithHighlightBorder() {
  return capture_mode::kHighlightLayerRadius +
         capture_mode::kInnerHightlightBorderThickness +
         capture_mode::kOuterHightlightBorderThickness;
}

// Returns the color used for the highlight layer affordance and border.
SkColor GetColor() {
  return capture_mode_util::GetColorProviderForNativeTheme()->GetColor(
      cros_tokens::kCrosSysOnSurface);
}

SkColor GetHighlightBorderInnerColor() {
  return capture_mode_util::GetColorProviderForNativeTheme()->GetColor(
      ui::kColorHighlightBorderHighlight1);
}

SkColor GetHighlightBorderOuterColor() {
  return capture_mode_util::GetColorProviderForNativeTheme()->GetColor(
      ui::kColorHighlightBorderBorder1);
}

}  // namespace

PointerHighlightLayer::PointerHighlightLayer(
    const gfx::PointF& event_location_in_window,
    ui::Layer* parent_layer) {
  DCHECK(parent_layer);
  SetLayer(std::make_unique<ui::Layer>(ui::LAYER_TEXTURED));
  layer()->SetFillsBoundsOpaquely(false);
  CenterAroundPoint(event_location_in_window);
  layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(capture_mode::kHighlightLayerRadius));
  layer()->set_delegate(this);
  layer()->SetName("PointerHighlightLayer");

  parent_layer->Add(layer());
  parent_layer->StackAtTop(layer());
}

PointerHighlightLayer::~PointerHighlightLayer() = default;

void PointerHighlightLayer::CenterAroundPoint(
    const gfx::PointF& event_location_in_window) {
  layer()->SetBounds(capture_mode_util::CalculateHighlightLayerBounds(
      event_location_in_window, (CalculateRadiusWithHighlightBorder())));
}

void PointerHighlightLayer::OnPaintLayer(const ui::PaintContext& context) {
  ui::PaintRecorder recorder(context, layer()->size());
  gfx::Canvas* canvas = recorder.canvas();
  gfx::ScopedCanvas scoped_canvas(canvas);
  const float dsf = canvas->UndoDeviceScaleFactor();
  const float scaled_highlight_radius =
      dsf * capture_mode::kHighlightLayerRadius;
  const float scaled_fills_radius = dsf * kFillsRadius;
  const gfx::PointF scaled_highlight_center = gfx::ConvertPointToPixels(
      capture_mode_util::GetLocalCenterPoint(layer()), dsf);
  cc::PaintFlags flags;
  const SkColor color = GetColor();

  // Draw the fills inside for the pointer highlight layer.
  // 50% opacity.
  flags.setColor(SkColorSetA(color, 128));
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->DrawCircle(scaled_highlight_center, scaled_highlight_radius, flags);

  // Draw the border outside of the pointer highlight layer.
  flags.setColor(
      SkColorSetA(color, DarkLightModeControllerImpl::Get()->IsDarkModeEnabled()
                             ? 255
                             : 255 * kLightModeBorderOpacityScaleFactor));
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(kHighlightStrokeWidth);
  canvas->DrawCircle(scaled_highlight_center, scaled_fills_radius, flags);

  // Draw circle highlight borders attached to the pointer highlight layer for
  // better visibility when the color of the pointer highlight has a low
  // contrast with the background.
  flags.setStrokeWidth(views::kHighlightBorderThickness);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setAntiAlias(true);
  flags.setColor(GetHighlightBorderInnerColor());
  const float scaled_inner_radius =
      dsf * (capture_mode::kHighlightLayerRadius +
             capture_mode::kInnerHightlightBorderThickness);
  canvas->DrawCircle(scaled_highlight_center, scaled_inner_radius, flags);

  flags.setColor(GetHighlightBorderOuterColor());
  const float scaled_outer_radius =
      dsf * (CalculateRadiusWithHighlightBorder());
  canvas->DrawCircle(scaled_highlight_center, scaled_outer_radius, flags);
}

}  // namespace ash