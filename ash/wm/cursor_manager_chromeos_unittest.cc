// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_switches.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
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

class ForceShowCursorManagerChromeosTest : public AshTestBase {
 public:
  ForceShowCursorManagerChromeosTest() = default;
  ForceShowCursorManagerChromeosTest(
      const ForceShowCursorManagerChromeosTest&) = delete;
  ForceShowCursorManagerChromeosTest& operator=(
      const ForceShowCursorManagerChromeosTest&) = delete;
  ~ForceShowCursorManagerChromeosTest() override = default;

  // AshTestBase:
  void SetUp() override {
    // Allow debug shortcuts so we can use the accelerator to force showing the
    // cursor.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kForceShowCursor);
    AshTestBase::SetUp();
  }
};

TEST_F(ForceShowCursorManagerChromeosTest, Basic) {
  auto* cursor_manager = Shell::Get()->cursor_manager();
  EXPECT_TRUE(cursor_manager->IsCursorVisible());

  cursor_manager->HideCursor();
  EXPECT_TRUE(cursor_manager->IsCursorVisible());

  Shell::Get()->SetCursorCompositingEnabled(true);
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
}

}  // namespace ash
