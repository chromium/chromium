// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/power_button.h"
#include "ash/system/unified/quick_settings_footer.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/view.h"

namespace ash {

// Pixel tests for the quick settings Power button and menu.
class PowerButtonPixelTest : public NoSessionAshTestBase {
 public:
  PowerButtonPixelTest() = default;

  // AshTestBase:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  void SetUp() override {
    NoSessionAshTestBase::SetUp();
    UnifiedSystemTray* system_tray = GetPrimaryUnifiedSystemTray();
    system_tray->ShowBubble();
    button_ = system_tray->bubble()
                  ->quick_settings_view()
                  ->footer_for_testing()
                  ->power_button_for_testing();
  }

 protected:
  views::View* GetMenuView() {
    return button_->GetMenuViewForTesting()->GetSubmenu();
  }

  PowerButton* GetPowerButton() { return button_; }

  // Simulates mouse press event on the power button.
  void SimulatePowerButtonPress() { LeftClickOn(button_->button_content_); }

 private:
  // Owned by view hierarchy.
  raw_ptr<PowerButton> button_ = nullptr;
};

// TODO(http://b/291573477): Re-enable this test.
TEST_F(PowerButtonPixelTest, DISABLED_NoSession) {
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "check_power_button",
      /*revision_number=*/2, GetPowerButton()));

  SimulatePowerButtonPress();
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "check_menu",
      /*revision_number=*/2, GetMenuView()));
}

// TODO(crbug.com/1451244): Re-enable this test.
TEST_F(PowerButtonPixelTest, DISABLED_LoginSession) {
  CreateUserSessions(1);

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "check_button",
      /*revision_number=*/0, GetPowerButton()));

  SimulatePowerButtonPress();
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "check_menu",
      /*revision_number=*/2, GetMenuView()));
}

// TODO(crbug.com/1451244): Re-enable this test.
TEST_F(PowerButtonPixelTest, DISABLED_LockScreenSession) {
  CreateUserSessions(1);
  BlockUserSession(BLOCKED_BY_LOCK_SCREEN);

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "check_button",
      /*revision_number=*/0, GetPowerButton()));

  SimulatePowerButtonPress();
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "check_menu",
      /*revision_number=*/2, GetMenuView()));
}

// TODO(crbug.com/1451244): Re-enable this test.
TEST_F(PowerButtonPixelTest, DISABLED_GuestMode) {
  SimulateGuestLogin();

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "check_button",
      /*revision_number=*/0, GetPowerButton()));

  SimulatePowerButtonPress();
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "check_menu",
      /*revision_number=*/2, GetMenuView()));
}
}  // namespace ash
