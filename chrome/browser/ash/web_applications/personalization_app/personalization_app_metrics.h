// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_PERSONALIZATION_APP_METRICS_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_PERSONALIZATION_APP_METRICS_H_

#include "ash/constants/ambient_theme.h"
#include "ash/constants/ambient_video.h"
#include "ash/constants/personalization_entry_point.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom-shared.h"

namespace ash::personalization_app {

// This enum is used to define the buckets for an enumerated UMA histogram.
// Hence,
//   (a) existing enumerated constants should never be deleted or reordered,
//   (b) new constants should only be appended at the end of the enumeration.
enum class ColorMode {
  // Light color mode.
  kLight = 0,
  // Dark color mode.
  kDark = 1,
  // Auto scheduling mode.
  kAuto = 2,
  kMaxValue = kAuto,
};

constexpr char kAmbientModeAnimationThemeHistogramName[] =
    "Ash.Personalization.AmbientMode.AnimationTheme2";
constexpr char kAmbientModeVideoHistogramName[] =
    "Ash.Personalization.AmbientMode.Video2";
constexpr char kPersonalizationThemeColorModeHistogramName[] =
    "Ash.Personalization.Theme.ColorMode";
constexpr char kPersonalizationKeyboardBacklightColorHistogramName[] =
    "Ash.Personalization.KeyboardBacklight.Color";

// -----------------------------------------------------------------------------
// Histograms
// -----------------------------------------------------------------------------

void LogPersonalizationTheme(ColorMode color_mode);

void LogAmbientModeTheme(ash::AmbientTheme animation_theme);

void LogAmbientModeVideo(ash::AmbientVideo video);

void LogPersonalizationEntryPoint(ash::PersonalizationEntryPoint entry_point);

void LogKeyboardBacklightColor(mojom::BacklightColor backlight_color);

}  // namespace ash::personalization_app

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_PERSONALIZATION_APP_METRICS_H_
