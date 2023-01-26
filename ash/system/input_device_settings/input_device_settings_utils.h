// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_UTILS_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_UTILS_H_

namespace ash {

// Checks if a given value is within the bounds set by `mojom::ModifierKey`.
bool IsValidModifier(int val);

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_UTILS_H_
