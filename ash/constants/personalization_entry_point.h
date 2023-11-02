// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CONSTANTS_PERSONALIZATION_ENTRY_POINT_H_
#define ASH_CONSTANTS_PERSONALIZATION_ENTRY_POINT_H_

namespace ash {

// Entry points lead to Personalization Hub.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PersonalizationEntryPoint {
  // Home screen right click.
  kHomeScreen = 0,
  // System tray/ Quick Settings.
  kSystemTray = 1,
  // Launcher search.
  kLauncherSearch = 2,
  // Settings
  kSettings = 3,
  // Settings search.
  kSettingsSearch = 4,
  // Keyboard brightness slider.
  kKeyboardBrightnessSlider = 5,
  kMaxValue = kKeyboardBrightnessSlider,
};

inline constexpr char kPersonalizationEntryPointHistogramName[] =
    "Ash.Personalization.EntryPoint";

}  // namespace ash

#endif  // ASH_CONSTANTS_PERSONALIZATION_ENTRY_POINT_H_
