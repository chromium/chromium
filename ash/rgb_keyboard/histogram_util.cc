// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/rgb_keyboard/histogram_util.h"

#include "base/metrics/histogram_functions.h"

namespace ash::rgb_keyboard::metrics {
void EmitRgbKeyboardCapabilityType(
    rgbkbd::RgbKeyboardCapabilities capabilities) {
  base::UmaHistogramEnumeration(kRgbKeyboardCapabilityTypeHistogramName,
                                RgbKeyboardCapabilityType(capabilities));
}
}  // namespace ash::rgb_keyboard::metrics
