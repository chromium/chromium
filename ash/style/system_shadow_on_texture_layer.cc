// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/system_shadow_on_texture_layer.h"

#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/skia_paint_util.h"

namespace ash {

SystemShadowOnTextureLayer::SystemShadowOnTextureLayer(SystemShadow::Type type)
    : shadow_values_(gfx::ShadowValue::MakeChromeOSSystemUIShadowValues(
          SystemShadow::GetElevationFromType(type))) {
  layer_.SetFillsBoundsOpaquely(false);
  layer_.set_delegate(this);
  layer_.SetBounds(GetLayerBounds());
}

SystemShadowOnTextureLayer::~SystemShadowOnTextureLayer() = default;

void SystemShadowOnTextureLayer::SetType(SystemShadow::Type type) {
  shadow_values_ = gfx::ShadowValue::MakeChromeOSSystemUIShadowValues(
      SystemShadow::GetElevationFromType(type));
  UpdateLayer();
}

void SystemShadowOnTextureLayer::SetContentBounds(const gfx::Rect& bounds) {
  content_bounds_ = bounds;
  layer_.SetBounds(GetLayerBounds());
  UpdateLayer();
}

void SystemShadowOnTextureLayer::SetRoundedCornerRadius(int corner_radius) {
  corner_radius_ = corner_radius;
  UpdateLayer();
}

const gfx::Rect& SystemShadowOnTextureLayer::GetContentBounds() {
  return content_bounds_;
}

ui::Layer* SystemShadowOnTextureLayer::GetLayer() {
  return &layer_;
}

ui::Layer* SystemShadowOnTextureLayer::GetNinePatchLayer() {
  return nullptr;
}

gfx::Rect SystemShadowOnTextureLayer::GetLayerBounds() const {
  gfx::Rect layer_bounds = content_bounds_;
  layer_bounds.Inset(gfx::ShadowValue::GetMargin(shadow_values_));
  return layer_bounds;
}

void SystemShadowOnTextureLayer::UpdateLayer() {
  layer_.SchedulePaint(GetLayerBounds());
}

void SystemShadowOnTextureLayer::OnPaintLayer(const ui::PaintContext& context) {
  // Create a rounded rect of content area.
  const gfx::Rect r_rect_bounds =
      content_bounds_ - GetLayerBounds().OffsetFromOrigin();
  const SkRRect r_rect = SkRRect::MakeRectXY(gfx::RectToSkRect(r_rect_bounds),
                                             corner_radius_, corner_radius_);

  // Clip out the center.
  ui::PaintRecorder recorder(context, content_bounds_.size());
  recorder.canvas()->sk_canvas()->clipRRect(r_rect, SkClipOp::kDifference,
                                            true);
  // Paint shadow.
  cc::PaintFlags shadow_flags;
  shadow_flags.setAntiAlias(true);
  shadow_flags.setLooper(gfx::CreateShadowDrawLooper(shadow_values_));
  // Due to anti alias, we should fill transparent color to the rounded corner
  // area.
  shadow_flags.setColor(SK_ColorTRANSPARENT);
  recorder.canvas()->sk_canvas()->drawRRect(r_rect, shadow_flags);
}

void SystemShadowOnTextureLayer::OnDeviceScaleFactorChanged(
    float old_device_scale_factor,
    float new_device_scale_factor) {}

}  // namespace ash
