// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_DEFAULTS_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_DEFAULTS_H_

#include "ash/constants/ash_constants.h"
#include "base/time/time.h"
#include "ui/events/ash/mojom/extended_fkeys_modifier.mojom-shared.h"
#include "ui/events/ash/mojom/simulate_right_click_modifier.mojom-shared.h"

namespace ash {

// TODO(dpad): Intended defaults for TopRowAreFKeys and SuppressMetaFKeyRewrites
// are different depending on internal vs external keyboard.
constexpr base::TimeDelta kDefaultAutoRepeatDelay = kDefaultKeyAutoRepeatDelay;
constexpr base::TimeDelta kDefaultAutoRepeatInterval =
    kDefaultKeyAutoRepeatInterval;
constexpr bool kDefaultAutoRepeatEnabled = true;
constexpr bool kDefaultSuppressMetaFKeyRewrites = false;
constexpr bool kDefaultTopRowAreFKeys = false;
constexpr bool kDefaultTopRowAreFKeysExternal = true;
constexpr ui::mojom::ExtendedFkeysModifier kDefaultFkey =
    ui::mojom::ExtendedFkeysModifier::kDisabled;

// Default settings for all pointers, defined in
// chrome/browser/ash/preferences/preferences.cc.
constexpr bool kDefaultSwapRight = false;
constexpr int kDefaultSensitivity = 3;
constexpr bool kDefaultReverseScrolling = false;
constexpr bool kDefaultAccelerationEnabled = true;
constexpr bool kDefaultScrollAccelerationEnabled = true;
constexpr int kDefaultScrollSensitivity = 3;

// Touchpad setting defaults.
constexpr bool kDefaultTapToClickEnabled = true;
constexpr bool kDefaultTapDraggingEnabled = false;
constexpr bool kDefaultThreeFingerClickEnabled = false;
constexpr bool kDefaultHapticFeedbackEnabled = false;
constexpr int kDefaultHapticSensitivity = 3;
constexpr ui::mojom::SimulateRightClickModifier kDefaultSimulateRightClick =
    ui::mojom::SimulateRightClickModifier::kNone;

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_DEFAULTS_H_
