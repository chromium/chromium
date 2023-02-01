// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/color_util.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "base/cxx17_backports.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/color_utils.h"

namespace ash {

namespace {

// Alpha value that is used to calculate themed color. Please see function
// GetBackgroundThemedColor() about how the themed color is calculated.
constexpr int kDarkBackgroundBlendAlpha = 127;   // 50%
constexpr int kLightBackgroundBlendAlpha = 127;  // 50%

// Alternate alpha values used when `kDarkLightModeKMeansColor` is active.
constexpr int kDarkBackgroundBlendKMeansAlpha = 165;   // 65%
constexpr int kLightBackgroundBlendKMeansAlpha = 230;  // 90%

// Clamp the lightness of input user colors so that there is sufficient contrast
// between shelf and wallpaper.
constexpr double kMaxLightnessLightMode = 0.7;
constexpr double kMinLightnessDarkMode = 0.3;

// The disabled color is always 38% opacity of the enabled color.
constexpr float kDisabledColorOpacity = 0.38f;

// Color of second tone is always 30% opacity of the color of first tone.
constexpr float kSecondToneOpacity = 0.3f;

// Get a color extracted from the user's wallpaper.
// Returns `kInvalidWallpaperColor` on failure.
// If `use_dark_color`, may attempt to extract a dark color from the wallpaper.
SkColor GetUserWallpaperColor(bool use_dark_color) {
  // May be null in unit tests.
  if (!Shell::HasInstance())
    return kInvalidWallpaperColor;

  WallpaperControllerImpl* wallpaper_controller =
      Shell::Get()->wallpaper_controller();

  if (!wallpaper_controller)
    return kInvalidWallpaperColor;

  const auto& calculated_colors = wallpaper_controller->calculated_colors();
  if (!calculated_colors) {
    return kInvalidWallpaperColor;
  }

  if (features::IsJellyEnabled()) {
    return calculated_colors->celebi_color;
  }

  if (features::IsDarkLightModeKMeansColorEnabled()) {
    // If feature is enabled, always use k mean color. Mixing with black/white
    // will handle adapting it to dark or light mode.
    return wallpaper_controller->GetKMeanColor();
  }

  color_utils::LumaRange luma_range = use_dark_color
                                          ? color_utils::LumaRange::DARK
                                          : color_utils::LumaRange::LIGHT;

  return wallpaper_controller->GetProminentColor(color_utils::ColorProfile(
      luma_range, color_utils::SaturationRange::MUTED));
}

int GetForegroundAlpha(bool use_dark_color) {
  if (features::IsDarkLightModeKMeansColorEnabled()) {
    return use_dark_color ? kDarkBackgroundBlendKMeansAlpha
                          : kLightBackgroundBlendKMeansAlpha;
  }
  return use_dark_color ? kDarkBackgroundBlendAlpha
                        : kLightBackgroundBlendAlpha;
}

SkColor ClampLightness(bool use_dark_color, SkColor color) {
  color_utils::HSL hsl;
  color_utils::SkColorToHSL(color, &hsl);

  if (use_dark_color) {
    hsl.l = base::clamp(hsl.l, kMinLightnessDarkMode, 1.0);
  } else {
    hsl.l = base::clamp(hsl.l, 0.0, kMaxLightnessLightMode);
  }
  return color_utils::HSLToSkColor(hsl, SkColorGetA(color));
}

}  // namespace

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
  const SkColor wallpaper_color = GetUserWallpaperColor(use_dark_color);
  if (wallpaper_color == kInvalidWallpaperColor) {
    DVLOG(1) << "Failed to get wallpaper color";
    return default_color;
  }

  if (features::IsJellyEnabled()) {
    return wallpaper_color;
  }

  const SkColor clamped_wallpaper_color =
      ClampLightness(use_dark_color, wallpaper_color);

  const SkColor foreground_color =
      use_dark_color ? SK_ColorBLACK : SK_ColorWHITE;

  const int foreground_alpha = GetForegroundAlpha(use_dark_color);

  // Put a slightly transparent screen of white/black on top of the user's
  // wallpaper color.
  return color_utils::GetResultingPaintColor(
      SkColorSetA(foreground_color, foreground_alpha), clamped_wallpaper_color);
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
