// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_UTILITY_HAPTICS_UTIL_H_
#define ASH_UTILITY_HAPTICS_UTIL_H_

#include "ash/ash_export.h"

namespace ui {
class InputController;
enum class HapticTouchpadEffect;
enum class HapticTouchpadEffectStrength;
}  // namespace ui

namespace ash {

// Utility that provides methods to trigger haptic effects throughout Ash.
// These call InputController functions that will play the effects if a haptic
// touchpad is available.
namespace haptics_util {

// Sets test input controller for testing. When g_test_input_controller is not
// nullptr, haptics_util::PlayHapticTouchpadEffect will call the test controller
// instead of the real one from ozone.
ASH_EXPORT void SetInputControllerForTesting(
    ui::InputController* input_controller);

// Plays a touchpad haptic feedback effect according to the given `effect` type,
// and the given `strength`. By default it uses ozone's input controller, unless
// it was overridden by the above SetInputControllerForTesting().
ASH_EXPORT void PlayHapticTouchpadEffect(
    ui::HapticTouchpadEffect effect,
    ui::HapticTouchpadEffectStrength strength);

// Plays a `ToggleOn` or `ToggleOff` haptic effect based on the `on` bool value.
ASH_EXPORT void PlayHapticToggleEffect(
    bool on,
    ui::HapticTouchpadEffectStrength strength);

}  // namespace haptics_util
}  // namespace ash

#endif  // ASH_UTILITY_HAPTICS_UTIL_H_
