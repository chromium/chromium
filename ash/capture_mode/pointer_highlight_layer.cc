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
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/scoped_canvas.h"

namespace ash {

namespace {

constexpr float kLightModeBorderOpacityScaleFactor = 0.8f;
const int kHighlightStrokeWidth = 2;
constexpr int kFillsRadius =
    capture_mode::kHighlightLayerRadius - kHighlightStrokeWidth;

// Returns the color used for the highlight layer affordance and border.
SkColor GetColor() {
  return capture_mode_util::GetColorProviderForNativeTheme()->GetColor(
      cros_tokens::kCrosSysOnSurface);
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
      event_location_in_window, capture_mode::kHighlightLayerRadius));
}

void PointerHighlightLayer::OnPaintLayer(const ui::PaintContext& context) {
  ui::PaintRecorder recorder(context, layer()->size());
  gfx::ScopedCanvas scoped_canvas(recorder.canvas());
  const float dsf = recorder.canvas()->UndoDeviceScaleFactor();
  const float scaled_highlight_radius =
      dsf * capture_mode::kHighlightLayerRadius;
  const float scaled_fills_radius = dsf * kFillsRadius;
  const gfx::PointF scaled_highlight_center = gfx::ConvertPointToPixels(
      capture_mode_util::GetLocalCenterPoint(layer()), dsf);

  cc::PaintFlags flags;
  const SkColor color = GetColor();

  // 50% opacity.
  flags.setColor(SkColorSetA(color, 128));
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  recorder.canvas()->DrawCircle(scaled_highlight_center,
                                scaled_highlight_radius, flags);

  flags.setColor(
      SkColorSetA(color, DarkLightModeControllerImpl::Get()->IsDarkModeEnabled()
                             ? 255
                             : 255 * kLightModeBorderOpacityScaleFactor));
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(kHighlightStrokeWidth);
  recorder.canvas()->DrawCircle(scaled_highlight_center, scaled_fills_radius,
                                flags);
}

}  // namespace ash