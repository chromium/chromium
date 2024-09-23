// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/rgb_keyboard/histogram_util.h"

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"

namespace ash::rgb_keyboard::metrics {
std::string GetCapabilityTypeStr(rgbkbd::RgbKeyboardCapabilities capabilities) {
  switch (capabilities) {
    case rgbkbd::RgbKeyboardCapabilities::kNone:
      NOTREACHED();
    case rgbkbd::RgbKeyboardCapabilities::kFourZoneFortyLed:
      return "FourZoneFortyLed";
    case rgbkbd::RgbKeyboardCapabilities::kIndividualKey:
      return "IndividualKey";
    case rgbkbd::RgbKeyboardCapabilities::kFourZoneTwelveLed:
      return "FourZoneTwelveLed";
    case rgbkbd::RgbKeyboardCapabilities::kFourZoneFourLed:
      return "FourZoneFourLed";
  }
}

void EmitRgbKeyboardCapabilityType(
    rgbkbd::RgbKeyboardCapabilities capabilities) {
  base::UmaHistogramEnumeration(kRgbKeyboardCapabilityTypeHistogramName,
                                RgbKeyboardCapabilityType(capabilities));
}

void EmitRgbBacklightChangeType(RgbKeyboardBacklightChangeType type,
                                rgbkbd::RgbKeyboardCapabilities capabilities) {
  const auto name = base::StrCat(
      {kRgbKeyboardHistogramPrefix, GetCapabilityTypeStr(capabilities)});
  base::UmaHistogramEnumeration(name, type);
}
}  // namespace ash::rgb_keyboard::metrics
