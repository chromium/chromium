// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/window_selector_controller.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "services/ws/public/cpp/input_devices/input_device_client_test_api.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/keyboard/test/keyboard_test_util.h"

namespace ash {

class WindowSelectorControllerTest : public AshTestBase {
 protected:
  void SetUp() override {
    AshTestBase::SetUp();

    ws::InputDeviceClientTestApi().SetKeyboardDevices({});
    ws::InputDeviceClientTestApi().SetTouchscreenDevices(
        {ui::TouchscreenDevice(1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
                               "Touchscreen", gfx::Size(1024, 768), 0)});

    TabletModeControllerTestApi().EnterTabletMode();
    ASSERT_TRUE(keyboard::IsKeyboardEnabled());

    // TODO(https://crbug.com/849995): Change |TestKeyboardUI| so that
    // it automatically notifies KeyboardController.
    keyboard_controller()->LoadKeyboardWindowInBackground();
    keyboard_controller()->GetKeyboardWindow()->SetBounds(
        keyboard::KeyboardBoundsFromRootBounds(
            Shell::GetPrimaryRootWindow()->bounds(), 100));
    keyboard_controller()->NotifyKeyboardWindowLoaded();
  }

  keyboard::KeyboardController* keyboard_controller() {
    return keyboard::KeyboardController::Get();
  }
};

TEST_F(WindowSelectorControllerTest, ToggleOverviewModeHidesVirtualKeyboard) {
  keyboard_controller()->ShowKeyboard(false /* locked */);
  keyboard::WaitUntilShown();

  Shell::Get()->window_selector_controller()->ToggleOverview();

  // Timeout failure here if the keyboard does not hide.
  keyboard::WaitUntilHidden();
}

TEST_F(WindowSelectorControllerTest,
       ToggleOverviewModeDoesNotHideLockedVirtualKeyboard) {
  keyboard_controller()->ShowKeyboard(true /* locked */);
  keyboard::WaitUntilShown();

  Shell::Get()->window_selector_controller()->ToggleOverview();
  EXPECT_FALSE(keyboard::IsKeyboardHiding());
}

}  // namespace ash
