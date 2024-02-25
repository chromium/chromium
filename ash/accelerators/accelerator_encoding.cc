// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_encoding.h"

#include "base/check.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ash {

// The encoding schema is as the following:
//
// - The low 16 bits represent the key code.
// - The modifiers are stored in the high 16 bits. Only the following 4 bits are
// being used:
//   - The 31 bit: Command key
//   - The 30 bit: Alt key
//   - The 29 bit: Control key
//   - The 28 bit: Shift key
//
// Examples:
//   ctrl+Z:        0001'0000'0000'0000'0000'0000'0101'1010
//   alt+shift+A:   0010'1000'0000'0000'0000'0000'0100'0001
int GetEncodedShortcut(const int modifiers, const ui::KeyboardCode key_code) {
  // EF_SHIFT_DOWN: 28th bit.
  const int kShiftDown = 1 << 27;
  // EF_CONTROL_DOWN: 29th bit.
  const int kControlDown = 1 << 28;
  // EF_ALT_DOWN: 30th bit.
  const int kAltDown = 1 << 29;
  // EF_COMMAND_DOWN: 31th bit.
  const int kCommandDown = 1 << 30;

  int encoded_modifier = 0;

  if ((modifiers & ui::EF_SHIFT_DOWN) != 0) {
    encoded_modifier |= kShiftDown;
  }
  if ((modifiers & ui::EF_CONTROL_DOWN) != 0) {
    encoded_modifier |= kControlDown;
  }
  if ((modifiers & ui::EF_ALT_DOWN) != 0) {
    encoded_modifier |= kAltDown;
  }
  if ((modifiers & ui::EF_COMMAND_DOWN) != 0) {
    encoded_modifier |= kCommandDown;
  }

  // Currently KeyboardCode only has 2^8 values. It will be a long time until we
  // get to 2^16. But if KeyboardCode has 2^28+ values for some reason, the top
  // 5 bits will be overwritten.
  DCHECK((0xF800 & key_code) == 0);
  return encoded_modifier | key_code;
}

}  // namespace ash
