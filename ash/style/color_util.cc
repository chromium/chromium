// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/color_util.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ui/gfx/color_analysis.h"

namespace ash {

// Alpha value that is used to calculate themed color. Please see function
// GetBackgroundThemedColor() about how the themed color is calculated.
constexpr int kDarkBackgroundBlendAlpha = 127;   // 50%
constexpr int kLightBackgroundBlendAlpha = 127;  // 50%

// The disabled color is always 38% opacity of the enabled color.
constexpr float kDisabledColorOpacity = 0.38f;

// Color of second tone is always 30% opacity of the color of first tone.
constexpr float kSecondToneOpacity = 0.3f;

// static
ui::ColorProviderSource* ColorUtil::GetColorProviderSourceForWindow(
    const aura::Window* window) {
  DCHECK(window);
  auto* root_window = window->GetRootWindow();
  if (!root_window)
    return nullptr;
  return RootWindowController::ForWindow(root_window)->color_provider_source();
}

// static
SkColor ColorUtil::GetBackgroundThemedColor(SkColor default_color,
                                            bool use_dark_color) {
  // May be null in unit tests.
  if (!Shell::HasInstance())
    return default_color;
  WallpaperControllerImpl* wallpaper_controller =
      Shell::Get()->wallpaper_controller();
  if (!wallpaper_controller)
    return default_color;

  color_utils::LumaRange luma_range = use_dark_color
                                          ? color_utils::LumaRange::DARK
                                          : color_utils::LumaRange::LIGHT;
  SkColor muted_color =
      wallpaper_controller->GetProminentColor(color_utils::ColorProfile(
          luma_range, color_utils::SaturationRange::MUTED));
  if (muted_color == kInvalidWallpaperColor)
    return default_color;

  return color_utils::GetResultingPaintColor(
      SkColorSetA(use_dark_color ? SK_ColorBLACK : SK_ColorWHITE,
                  use_dark_color ? kDarkBackgroundBlendAlpha
                                 : kLightBackgroundBlendAlpha),
      muted_color);
}

// static
SkColor ColorUtil::GetDisabledColor(SkColor enabled_color) {
  return SkColorSetA(enabled_color, std::round(SkColorGetA(enabled_color) *
                                               kDisabledColorOpacity));
}

// static
SkColor ColorUtil::GetSecondToneColor(SkColor color_of_first_tone) {
  return SkColorSetA(
      color_of_first_tone,
      std::round(SkColorGetA(color_of_first_tone) * kSecondToneOpacity));
}

}  // namespace ash
