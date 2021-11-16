// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_HAPTICS_UTIL_H_
#define ASH_WM_HAPTICS_UTIL_H_

#include "ash/ash_export.h"

namespace ui {
class InputController;
enum class HapticTouchpadEffect;
enum class HapticTouchpadEffectStrength;
}  // namespace ui

namespace ash {

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

}  // namespace haptics_util
}  // namespace ash

#endif  // ASH_WM_HAPTICS_UTIL_H_
