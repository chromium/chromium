// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/default_colors.h"

#include "ash/public/cpp/ash_features.h"

namespace ash {

SkColor DeprecatedGetShieldLayerColor(AshColorProvider::ShieldLayerType type,
                                      SkColor default_color) {
  if (!features::IsDarkLightModeEnabled())
    return default_color;

  return AshColorProvider::Get()->GetShieldLayerColor(type);
}

SkColor DeprecatedGetBaseLayerColor(AshColorProvider::BaseLayerType type,
                                    SkColor default_color) {
  if (!features::IsDarkLightModeEnabled())
    return default_color;

  return AshColorProvider::Get()->GetBaseLayerColor(type);
}

SkColor DeprecatedGetControlsLayerColor(
    AshColorProvider::ControlsLayerType type,
    SkColor default_color) {
  if (!features::IsDarkLightModeEnabled())
    return default_color;

  return AshColorProvider::Get()->GetControlsLayerColor(type);
}

SkColor DeprecatedGetContentLayerColor(AshColorProvider::ContentLayerType type,
                                       SkColor default_color) {
  if (!features::IsDarkLightModeEnabled())
    return default_color;

  return AshColorProvider::Get()->GetContentLayerColor(type);
}

SkColor DeprecatedGetLoginBackgroundBaseColor(SkColor default_color) {
  if (!features::IsDarkLightModeEnabled())
    return default_color;
  return AshColorProvider::Get()->GetLoginBackgroundBaseColor();
}

SkColor DeprecatedGetInkDropBaseColor(SkColor default_color) {
  if (!features::IsDarkLightModeEnabled())
    return default_color;

  return AshColorProvider::Get()->GetRippleAttributes().base_color;
}

SkColor DeprecatedGetInkDropRippleColor(SkColor default_color) {
  if (!features::IsDarkLightModeEnabled())
    return default_color;

  AshColorProvider::RippleAttributes ripple_attributes =
      AshColorProvider::Get()->GetRippleAttributes(
          AshColorProvider::Get()->GetShieldLayerColor(
              AshColorProvider::ShieldLayerType::kShield80));
  return SkColorSetA(ripple_attributes.base_color,
                     ripple_attributes.inkdrop_opacity * 255);
}

SkColor DeprecatedGetInkDropHighlightColor(SkColor default_color) {
  if (!features::IsDarkLightModeEnabled())
    return default_color;

  AshColorProvider::RippleAttributes ripple_attributes =
      AshColorProvider::Get()->GetRippleAttributes(
          AshColorProvider::Get()->GetShieldLayerColor(
              AshColorProvider::ShieldLayerType::kShield80));
  return SkColorSetA(ripple_attributes.base_color,
                     ripple_attributes.inkdrop_opacity * 255);
}

float DeprecatedGetInkDropOpacity(float default_opacity) {
  if (!features::IsDarkLightModeEnabled())
    return default_opacity;

  return AshColorProvider::Get()->GetRippleAttributes().inkdrop_opacity;
}

SkColor DeprecatedGetAppStateIndicatorColor(bool active,
                                            SkColor active_color,
                                            SkColor default_color) {
  if (!features::IsDarkLightModeEnabled())
    return active ? active_color : default_color;

  return AshColorProvider::Get()->GetContentLayerColor(
      active ? AshColorProvider::ContentLayerType::kAppStateIndicatorColor
             : AshColorProvider::ContentLayerType::
                   kAppStateIndicatorColorInactive);
}

}  // namespace ash
