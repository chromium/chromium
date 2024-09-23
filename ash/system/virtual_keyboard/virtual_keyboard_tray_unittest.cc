// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/virtual_keyboard/virtual_keyboard_tray.h"

#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/keyboard_util.h"
#include "ash/keyboard/ui/test/keyboard_test_util.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

class VirtualKeyboardTrayTest : public AshTestBase {
 protected:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);
    AshTestBase::SetUp();
    ASSERT_TRUE(keyboard::IsKeyboardEnabled());
    keyboard::test::WaitUntilLoaded();

    // These tests only apply to the floating virtual keyboard, as it is the
    // only case where both the virtual keyboard and the shelf are visible.
    keyboard_controller()->SetContainerType(keyboard::ContainerType::kFloating,
                                            gfx::Rect(), base::DoNothing());
  }

  keyboard::KeyboardUIController* keyboard_controller() {
    return keyboard::KeyboardUIController::Get();
  }
};

// Tests that the tray action toggles the virtual keyboard.
TEST_F(VirtualKeyboardTrayTest, PerformActionTogglesVirtualKeyboard) {
  StatusAreaWidget* status = StatusAreaWidgetTestHelper::GetStatusAreaWidget();
  VirtualKeyboardTray* tray = status->virtual_keyboard_tray_for_testing();
  tray->SetVisiblePreferred(true);
  ASSERT_TRUE(tray->GetVisible());

  // First tap should show the virtual keyboard.
  GestureTapOn(tray);
  EXPECT_TRUE(tray->is_active());
  ASSERT_TRUE(keyboard::test::WaitUntilShown());

  // Second tap should hide the virtual keyboard.
  GestureTapOn(tray);
  EXPECT_FALSE(tray->is_active());
  ASSERT_TRUE(keyboard::test::WaitUntilHidden());
}

}  // namespace ash
