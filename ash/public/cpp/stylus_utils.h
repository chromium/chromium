// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_STYLUS_UTILS_H_
#define ASH_PUBLIC_CPP_STYLUS_UTILS_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {
namespace stylus_utils {

// Returns true if the flag to force stylus input is set to true.
ASH_PUBLIC_EXPORT bool HasForcedStylusInput();

// Returns true if there is a stylus input device on the internal display. This
// will return false even if there is a stylus input device until hardware
// probing is complete (see ui::InputDeviceEventObserver).
ASH_PUBLIC_EXPORT bool HasStylusInput();

// Returns true if the palette should be shown on every display.
ASH_PUBLIC_EXPORT bool IsPaletteEnabledOnEveryDisplay();

// Returns true if the device has an internal stylus.
ASH_PUBLIC_EXPORT bool HasInternalStylus();

// Forcibly say the device has stylus input for testing purposes.
ASH_PUBLIC_EXPORT void SetHasStylusInputForTesting();

// Forcibly say the device doesn't have stylus input for testing purposes.
ASH_PUBLIC_EXPORT void SetNoStylusInputForTesting();

}  // namespace stylus_utils
}  // namespace ash

#endif  // ASH_PUBLIC_CPP_STYLUS_UTILS_H_
