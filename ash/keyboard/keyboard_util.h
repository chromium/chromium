// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KEYBOARD_KEYBOARD_UTIL_H_
#define ASH_KEYBOARD_KEYBOARD_UTIL_H_

#include "ash/ash_export.h"

#include "ui/events/keycodes/keyboard_codes.h"

namespace ash {

namespace keyboard_util {

// Returns whether the given key code corresponds to one of the 4 arrow keys.
ASH_EXPORT bool IsArrowKeyCode(const ui::KeyboardCode key_code);

}  // namespace keyboard_util

}  // namespace ash

#endif  // ASH_KEYBOARD_KEYBOARD_UTIL_H_
