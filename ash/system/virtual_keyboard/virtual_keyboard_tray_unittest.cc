// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/virtual_keyboard/virtual_keyboard_tray.h"

#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_switches.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/keyboard/test/keyboard_test_util.h"

namespace ash {

class VirtualKeyboardTrayTest : public AshTestBase {
 protected:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);
    AshTestBase::SetUp();
    ASSERT_TRUE(keyboard::IsKeyboardEnabled());

    // These tests only apply to the floating virtual keyboard, as it is the
    // only case where both the virtual keyboard and the shelf are visible.
    keyboard_controller()->LoadKeyboardWindowInBackground();
    keyboard_controller()->NotifyKeyboardWindowLoaded();
    keyboard_controller()->SetContainerType(keyboard::ContainerType::FLOATING,
                                            base::nullopt, base::DoNothing());
  }

  keyboard::KeyboardController* keyboard_controller() {
    return keyboard::KeyboardController::Get();
  }
};

// Tests that the tray action toggles the virtual keyboard.
TEST_F(VirtualKeyboardTrayTest, PerformActionTogglesVirtualKeyboard) {
  StatusAreaWidget* status = StatusAreaWidgetTestHelper::GetStatusAreaWidget();
  VirtualKeyboardTray* tray = status->virtual_keyboard_tray_for_testing();
  tray->SetVisible(true);
  ASSERT_TRUE(tray->visible());

  // First tap should show the virtual keyboard.
  tray->PerformAction(ui::GestureEvent(
      0, 0, 0, base::TimeTicks(), ui::GestureEventDetails(ui::ET_GESTURE_TAP)));
  EXPECT_TRUE(tray->is_active());
  ASSERT_TRUE(keyboard::WaitUntilShown());

  // Second tap should hide the virtual keyboard.
  tray->PerformAction(ui::GestureEvent(
      0, 0, 0, base::TimeTicks(), ui::GestureEventDetails(ui::ET_GESTURE_TAP)));
  EXPECT_FALSE(tray->is_active());
  ASSERT_TRUE(keyboard::WaitUntilHidden());
}

}  // namespace ash
