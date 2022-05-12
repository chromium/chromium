// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_PERSONALIZATION_APP_METRICS_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_PERSONALIZATION_APP_METRICS_H_

#include "ash/constants/ambient_animation_theme.h"
#include "ash/constants/personalization_entry_point.h"

namespace ash {
namespace personalization_app {

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
    "Ash.Personalization.AmbientMode.AnimationTheme";
constexpr char kPersonalizationThemeColorModeHistogramName[] =
    "Ash.Personalization.Theme.ColorMode";

// -----------------------------------------------------------------------------
// Histograms
// -----------------------------------------------------------------------------

void LogPersonalizationTheme(ColorMode color_mode);

void LogAmbientModeAnimationTheme(ash::AmbientAnimationTheme animation_theme);

void LogPersonalizationEntryPoint(ash::PersonalizationEntryPoint entry_point);

}  // namespace personalization_app
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_PERSONALIZATION_APP_METRICS_H_
