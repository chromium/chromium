// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_RGB_KEYBOARD_HISTOGRAM_UTIL_H_
#define ASH_RGB_KEYBOARD_HISTOGRAM_UTIL_H_

#include <string>

#include "ash/ash_export.h"
#include "third_party/cros_system_api/dbus/rgbkbd/dbus-constants.h"

namespace ash::rgb_keyboard::metrics {
constexpr char kRgbKeyboardHistogramPrefix[] = "ChromeOS.RgbKeyboard.";
constexpr char kRgbKeyboardCapabilityTypeHistogramName[] =
    "ChromeOS.RgbKeyboard.RgbKeyboardCapabilityType";

// The enums below are used in histograms, do not remove/renumber entries. If
// you're adding to any of these enums, update the corresponding enum listing in
// tools/metrics/histograms/enums.xml:
// RgbKeyboardCapabilityType/RgbKeyboardBacklightChangeType.
// RgbKeyboardCapabilityType gets its values from RgbKeyboardCapabilities
// defined in: third_party/cros_system_api/dbus/rgbkbd/dbus-constants.h.
enum class RgbKeyboardCapabilityType {
  kNone = 0,
  kIndividualKey = 1,
  kFourZoneFortyLed = 2,
  kFourZoneTwelveLed = 3,
  kFourZoneFourLed = 4,
  kMaxValue = kFourZoneFourLed,
};

enum class RgbKeyboardBacklightChangeType {
  kStaticBackgroundColorChanged = 0,
  kRainbowModeSelected = 1,
  kStaticZoneColorChanged = 2,
  kMaxValue = kStaticZoneColorChanged,
};

ASH_EXPORT std::string GetCapabilityTypeStr(
    rgbkbd::RgbKeyboardCapabilities capabilities);

void EmitRgbKeyboardCapabilityType(
    rgbkbd::RgbKeyboardCapabilities capabilities);

void EmitRgbBacklightChangeType(RgbKeyboardBacklightChangeType type,
                                rgbkbd::RgbKeyboardCapabilities capabilities);

}  // namespace ash::rgb_keyboard::metrics

#endif  // ASH_RGB_KEYBOARD_HISTOGRAM_UTIL_H_
