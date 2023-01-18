// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_DEFAULTS_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_DEFAULTS_H_

#include "ash/constants/ash_constants.h"
#include "base/time/time.h"

namespace ash {

// TODO(dpad): Intended defaults for TopRowAreFKeys and SuppressMetaFKeyRewrites
// are different depending on internal vs external keyboard.
constexpr base::TimeDelta kDefaultAutoRepeatDelay = kDefaultKeyAutoRepeatDelay;
constexpr base::TimeDelta kDefaultAutoRepeatInterval =
    kDefaultKeyAutoRepeatInterval;
constexpr bool kDefaultAutoRepeatEnabled = true;
constexpr bool kDefaultSuppressMetaFKeyRewrites = false;
constexpr bool kDefaultTopRowAreFKeys = false;

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_DEFAULTS_H_
