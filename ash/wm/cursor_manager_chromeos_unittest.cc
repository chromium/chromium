// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ui/wm/core/cursor_manager.h"

namespace ash {

using CursorManagerChromeosTest = AshTestBase;

// Verifies the cursor's visibility after receiving key commands.
TEST_F(CursorManagerChromeosTest, VerifyVisibilityAfterKeyCommands) {
  auto* cursor_manager = Shell::Get()->cursor_manager();
  ASSERT_TRUE(cursor_manager->IsCursorVisible());

  // Pressing the normal key should hide the cursor.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_C);
  EXPECT_FALSE(cursor_manager->IsCursorVisible());

  // Move the mouse and the cursor should show.
  GetEventGenerator()->MoveMouseBy(/*x=*/1, /*y=*/1);
  EXPECT_TRUE(cursor_manager->IsCursorVisible());

  // The command key commands should not hide the cursor.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_C, ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(cursor_manager->IsCursorVisible());

  // The alt key commands should not hide the cursor.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_C, ui::EF_ALT_DOWN);
  EXPECT_TRUE(cursor_manager->IsCursorVisible());

  // The control key commands should not hide the cursor.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_C, ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
}

}  // namespace ash
