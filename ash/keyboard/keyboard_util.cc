// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/keyboard_util.h"

namespace ash {

namespace keyboard_util {

bool IsArrowKeyCode(const ui::KeyboardCode key_code) {
  return key_code == ui::VKEY_DOWN || key_code == ui::VKEY_RIGHT ||
         key_code == ui::VKEY_LEFT || key_code == ui::VKEY_UP;
}

}  // namespace keyboard_util

}  // namespace ash
