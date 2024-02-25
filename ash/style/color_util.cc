// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/color_util.h"

#include <algorithm>

#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "chromeos/constants/chromeos_features.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_utils.h"

namespace ash {

namespace {

// Alpha value that is used to calculate themed color. Please see function
// GetBackgroundThemedColor() about how the themed color is calculated.
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

SkColor ClampLightness(bool use_dark_color, SkColor color) {
  color_utils::HSL hsl;
  color_utils::SkColorToHSL(color, &hsl);

  if (use_dark_color) {
    hsl.l = std::clamp(hsl.l, kMinLightnessDarkMode, 1.0);
  } else {
    hsl.l = std::clamp(hsl.l, 0.0, kMaxLightnessLightMode);
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
SkColor ColorUtil::AdjustKMeansColor(SkColor k_means_color,
                                     bool use_dark_color) {
  const SkColor clamped_k_means_color =
      ClampLightness(use_dark_color, k_means_color);

  const SkColor foreground_color =
      use_dark_color ? SK_ColorBLACK : SK_ColorWHITE;

  const int foreground_alpha = use_dark_color
                                   ? kDarkBackgroundBlendKMeansAlpha
                                   : kLightBackgroundBlendKMeansAlpha;

  // Put a slightly transparent screen of white/black on top of the user's
  // wallpaper color.
  return color_utils::GetResultingPaintColor(
      SkColorSetA(foreground_color, foreground_alpha), clamped_k_means_color);
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
