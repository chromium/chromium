// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/system_shadow_on_texture_layer.h"

#include "ash/style/style_util.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/skia_paint_util.h"

namespace ash {

SystemShadowOnTextureLayer::SystemShadowOnTextureLayer(SystemShadow::Type type)
    : type_(type),
      shadow_values_(gfx::ShadowValue::MakeChromeOSSystemUIShadowValues(
          SystemShadow::GetElevationFromType(type))) {
  layer_.SetFillsBoundsOpaquely(false);
  layer_.set_delegate(this);
  layer_.SetBounds(GetLayerBounds());
}

SystemShadowOnTextureLayer::~SystemShadowOnTextureLayer() = default;

void SystemShadowOnTextureLayer::SetType(SystemShadow::Type type) {
  type_ = type;
  UpdateShadowValues();
  UpdateLayer();
}

void SystemShadowOnTextureLayer::SetContentBounds(const gfx::Rect& bounds) {
  content_bounds_ = bounds;
  layer_.SetBounds(GetLayerBounds());
  UpdateLayer();
}

void SystemShadowOnTextureLayer::SetRoundedCornerRadius(int corner_radius) {
  SetRoundedCorners(gfx::RoundedCornersF(corner_radius));
}

void SystemShadowOnTextureLayer::SetRoundedCorners(
    const gfx::RoundedCornersF& rounded_corners) {
  if (rounded_corners_ == rounded_corners) {
    return;
  }

  rounded_corners_ = rounded_corners;
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

const gfx::ShadowValues SystemShadowOnTextureLayer::GetShadowValuesForTesting()
    const {
  return shadow_values_;
}

void SystemShadowOnTextureLayer::UpdateLayer() {
  layer_.SchedulePaint(GetLayerBounds());
}

void SystemShadowOnTextureLayer::UpdateShadowValues() {
  const int elevation = SystemShadow::GetElevationFromType(type_);
  auto iter = colors_map_.find(elevation);
  if (iter == colors_map_.end()) {
    shadow_values_ =
        gfx::ShadowValue::MakeChromeOSSystemUIShadowValues(elevation);
  } else {
    shadow_values_ = gfx::ShadowValue::MakeChromeOSSystemUIShadowValues(
        elevation, /*key_shadow_color=*/iter->second.first,
        /*second_color=*/iter->second.second);
  }
}

void SystemShadowOnTextureLayer::OnPaintLayer(const ui::PaintContext& context) {
  // Create a rounded rect of content area.
  const gfx::Rect r_rect_bounds =
      content_bounds_ - GetLayerBounds().OffsetFromOrigin();
  SkRRect r_rect;
  float upper_left = rounded_corners_.upper_left();
  float upper_right = rounded_corners_.upper_right();
  float lower_right = rounded_corners_.lower_right();
  float lower_left = rounded_corners_.lower_left();
  SkVector radii[4] = {{upper_left, upper_left},
                       {upper_right, upper_right},
                       {lower_right, lower_right},
                       {lower_left, lower_left}};
  r_rect.setRectRadii(gfx::RectToSkRect(r_rect_bounds), radii);

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

void SystemShadowOnTextureLayer::UpdateShadowColors(
    const ui::ColorProvider* color_provider) {
  colors_map_ = StyleUtil::CreateShadowElevationToColorsMap(color_provider);
  UpdateShadowValues();
  UpdateLayer();
}

}  // namespace ash
