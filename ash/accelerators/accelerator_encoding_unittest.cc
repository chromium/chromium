// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_encoding.h"

#include "ash/test/ash_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {

class AcceleratorEncodingTest : public AshTestBase {
 public:
  AcceleratorEncodingTest() = default;
  ~AcceleratorEncodingTest() override = default;
};

// Test that GetEncodedShortcut encodes a shortcut correctly.
// - The low 16 bits represent the key code.
// - The high 16 bits represent the modififers.
//   - The 31 bit: Command key
//   - The 30 bit: Alt key
//   - The 29 bit: Control key
//   - The 28 bit: Shift key
//   - All other bits are 0
TEST_F(AcceleratorEncodingTest, GetEncodedShortcut) {
  // Test will verify that ui::EF_FUNCTION_DOWN and ui::EF_ALTGR_DOWN will be
  // ignored.
  const int all_modifiers = ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                            ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN |
                            ui::EF_FUNCTION_DOWN | ui::EF_ALTGR_DOWN;
  struct {
    ui::KeyboardCode code;
    int modifiers;
    const int expected_int;
  } keys[] = {
      {ui::VKEY_A, ui::EF_SHIFT_DOWN, 0x0800'0041},  // A: 0x41
      {ui::VKEY_A, ui::EF_CONTROL_DOWN, 0x1000'0041},
      {ui::VKEY_A, ui::EF_ALT_DOWN, 0x2000'0041},
      {ui::VKEY_A, ui::EF_COMMAND_DOWN, 0x4000'0041},
      {ui::VKEY_Z, all_modifiers, 0x7800'005A},  // Z: 0x5A
  };

  for (const auto& key : keys) {
    EXPECT_EQ(GetEncodedShortcut(key.modifiers, key.code), key.expected_int);
  }
}

}  // namespace ash
