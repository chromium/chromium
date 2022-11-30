// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_metrics.h"
#include "ash/constants/ambient_animation_theme.h"
#include "base/metrics/histogram_functions.h"

namespace ash::personalization_app {

void LogPersonalizationTheme(ColorMode color_mode) {
  base::UmaHistogramEnumeration(kPersonalizationThemeColorModeHistogramName,
                                color_mode);
}

void LogAmbientModeAnimationTheme(ash::AmbientAnimationTheme animation_theme) {
  base::UmaHistogramEnumeration(kAmbientModeAnimationThemeHistogramName,
                                animation_theme);
}

void LogPersonalizationEntryPoint(ash::PersonalizationEntryPoint entry_point) {
  base::UmaHistogramEnumeration(ash::kPersonalizationEntryPointHistogramName,
                                entry_point);
}

void LogKeyboardBacklightColor(mojom::BacklightColor backlight_color) {
  base::UmaHistogramEnumeration(
      kPersonalizationKeyboardBacklightColorHistogramName, backlight_color);
}

}  // namespace ash::personalization_app
