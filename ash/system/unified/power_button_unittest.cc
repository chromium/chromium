// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/power_button.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/style/icon_button.h"
#include "ash/system/unified/quick_settings_footer.h"
#include "ash/system/unified/quick_settings_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/user_chooser_view.h"
#include "ash/test/ash_test_base.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/user_manager/user_type.h"
#include "ui/compositor/layer.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace ash {

// Tests for `PowerButton`, which is inited with no user session to test the
// non-login state.
class PowerButtonTest : public NoSessionAshTestBase {
 public:
  PowerButtonTest() = default;
  PowerButtonTest(const PowerButtonTest&) = delete;
  PowerButtonTest& operator=(const PowerButtonTest&) = delete;
  ~PowerButtonTest() override = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeature(features::kQsRevamp);
    NoSessionAshTestBase::SetUp();
    // Test with the real system tray bubble so that the power button has a real
    // UnifiedSystemTrayController to test clicking on the email item.
    UnifiedSystemTray* system_tray = GetPrimaryUnifiedSystemTray();
    system_tray->ShowBubble();
    button_ = system_tray->bubble()
                  ->quick_settings_view()
                  ->footer_for_testing()
                  ->power_button_for_testing();
  }

 protected:
  views::MenuItemView* GetMenuView() {
    return button_->GetMenuViewForTesting();
  }

  bool IsMenuShowing() { return button_->IsMenuShowing(); }

  views::MenuItemView* GetEmailButton() {
    if (!IsMenuShowing()) {
      return nullptr;
    }
    return GetMenuView()->GetMenuItemByID(VIEW_ID_QS_POWER_EMAIL_MENU_BUTTON);
  }

  views::View* GetRestartButton() {
    if (!IsMenuShowing()) {
      return nullptr;
    }
    return GetMenuView()->GetMenuItemByID(VIEW_ID_QS_POWER_RESTART_MENU_BUTTON);
  }

  views::View* GetPowerOffButton() {
    if (!IsMenuShowing()) {
      return nullptr;
    }
    return GetMenuView()->GetMenuItemByID(VIEW_ID_QS_POWER_OFF_MENU_BUTTON);
  }

  views::View* GetSignOutButton() {
    if (!IsMenuShowing()) {
      return nullptr;
    }
    return GetMenuView()->GetMenuItemByID(VIEW_ID_QS_POWER_SIGNOUT_MENU_BUTTON);
  }

  views::View* GetLockButton() {
    if (!IsMenuShowing()) {
      return nullptr;
    }
    return GetMenuView()->GetMenuItemByID(VIEW_ID_QS_POWER_LOCK_MENU_BUTTON);
  }

  PowerButton* GetPowerButton() { return button_; }

  ui::Layer* GetBackgroundLayer() { return button_->background_view_->layer(); }

  // Simulates mouse press event on the power button. The generator click
  // does not work anymore since menu is a nested run loop.
  void SimulatePowerButtonPress() {
    ui::MouseEvent event(ui::ET_MOUSE_PRESSED,
                         button_->GetBoundsInScreen().CenterPoint(),
                         button_->GetBoundsInScreen().CenterPoint(),
                         ui::EventTimeForNow(), 0, 0);
    button_->button_content_->NotifyClick(event);
  }

  // Owned by view hierarchy.
  raw_ptr<PowerButton, ExperimentalAsh> button_ = nullptr;

  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;
};

// `PowerButton` should be with the correct view id and have the UMA tracking
// with the correct catalog name.
TEST_F(PowerButtonTest, PowerButtonHasCorrectViewIdAndUma) {
  CreateUserSessions(1);

  // No metrics logged before clicking on any buttons.
  histogram_tester_.ExpectTotalCount("Ash.QuickSettings.Button.Activated",
                                     /*count=*/0);

  // The power button is visible and with the corresponding id.
  EXPECT_TRUE(GetPowerButton()->GetVisible());
  EXPECT_EQ(VIEW_ID_QS_POWER_BUTTON, GetPowerButton()->GetID());

  // Clicks on the power button.
  SimulatePowerButtonPress();
  EXPECT_TRUE(IsMenuShowing());

  histogram_tester_.ExpectTotalCount("Ash.QuickSettings.Button.Activated",
                                     /*count=*/1);
  histogram_tester_.ExpectBucketCount("Ash.QuickSettings.Button.Activated",
                                      QsButtonCatalogName::kPowerButton,
                                      /*expected_count=*/1);
}

TEST_F(PowerButtonTest, LockMenuButtonRecordsUma) {
  CreateUserSessions(1);
  SimulatePowerButtonPress();

  LeftClickOn(GetLockButton());

  // Expect a count of 2 because the power button was activated above.
  histogram_tester_.ExpectTotalCount("Ash.QuickSettings.Button.Activated",
                                     /*count=*/2);
  histogram_tester_.ExpectBucketCount("Ash.QuickSettings.Button.Activated",
                                      QsButtonCatalogName::kPowerLockMenuButton,
                                      /*expected_count=*/1);
}

TEST_F(PowerButtonTest, SignOutMenuButtonRecordsUma) {
  CreateUserSessions(1);
  SimulatePowerButtonPress();

  LeftClickOn(GetSignOutButton());

  // Expect a count of 2 because the power button was activated above.
  histogram_tester_.ExpectTotalCount("Ash.QuickSettings.Button.Activated",
                                     /*count=*/2);
  histogram_tester_.ExpectBucketCount(
      "Ash.QuickSettings.Button.Activated",
      QsButtonCatalogName::kPowerSignoutMenuButton,
      /*expected_count=*/1);
}

TEST_F(PowerButtonTest, RestartMenuButtonRecordsUma) {
  CreateUserSessions(1);
  SimulatePowerButtonPress();

  LeftClickOn(GetRestartButton());

  // Expect a count of 2 because the power button was activated above.
  histogram_tester_.ExpectTotalCount("Ash.QuickSettings.Button.Activated",
                                     /*count=*/2);
  histogram_tester_.ExpectBucketCount(
      "Ash.QuickSettings.Button.Activated",
      QsButtonCatalogName::kPowerRestartMenuButton,
      /*expected_count=*/1);
}

TEST_F(PowerButtonTest, PowerOffMenuButtonRecordsUma) {
  CreateUserSessions(1);
  SimulatePowerButtonPress();

  LeftClickOn(GetPowerOffButton());

  // Expect a count of 2 because the power button was activated above.
  histogram_tester_.ExpectTotalCount("Ash.QuickSettings.Button.Activated",
                                     /*count=*/2);
  histogram_tester_.ExpectBucketCount("Ash.QuickSettings.Button.Activated",
                                      QsButtonCatalogName::kPowerOffMenuButton,
                                      /*expected_count=*/1);
}

TEST_F(PowerButtonTest, EmailMenuButtonRecordsUma) {
  CreateUserSessions(1);
  SimulatePowerButtonPress();

  LeftClickOn(GetEmailButton());

  // Expect a count of 2 because the power button was activated above.
  histogram_tester_.ExpectTotalCount("Ash.QuickSettings.Button.Activated",
                                     /*count=*/2);
  histogram_tester_.ExpectBucketCount(
      "Ash.QuickSettings.Button.Activated",
      QsButtonCatalogName::kPowerEmailMenuButton,
      /*expected_count=*/1);
}

// No lock and sign out buttons in the menu before login.
TEST_F(PowerButtonTest, ButtonStatesNotLoggedIn) {
  EXPECT_TRUE(GetPowerButton()->GetVisible());

  // No menu buttons are visible before showing the menu.
  EXPECT_FALSE(IsMenuShowing());
  EXPECT_EQ(nullptr, GetEmailButton());
  EXPECT_EQ(nullptr, GetRestartButton());
  EXPECT_EQ(nullptr, GetSignOutButton());
  EXPECT_EQ(nullptr, GetLockButton());
  EXPECT_EQ(nullptr, GetPowerOffButton());

  // Clicks on the power button.
  SimulatePowerButtonPress();

  EXPECT_TRUE(IsMenuShowing());

  // Only show power off and restart buttons.
  EXPECT_EQ(nullptr, GetEmailButton());
  EXPECT_EQ(nullptr, GetSignOutButton());
  EXPECT_EQ(nullptr, GetLockButton());
  EXPECT_TRUE(GetPowerOffButton()->GetVisible());
  EXPECT_TRUE(GetRestartButton()->GetVisible());
}

// All buttons are shown after login.
TEST_F(PowerButtonTest, ButtonStatesLoggedIn) {
  CreateUserSessions(1);

  EXPECT_TRUE(GetPowerButton()->GetVisible());

  // No menu buttons are visible before showing the menu.
  EXPECT_FALSE(IsMenuShowing());

  EXPECT_EQ(nullptr, GetEmailButton());
  EXPECT_EQ(nullptr, GetRestartButton());
  EXPECT_EQ(nullptr, GetSignOutButton());
  EXPECT_EQ(nullptr, GetLockButton());
  EXPECT_EQ(nullptr, GetPowerOffButton());

  // Clicks on the power button.
  SimulatePowerButtonPress();

  EXPECT_TRUE(IsMenuShowing());

  // Show all buttons in the menu.
  EXPECT_TRUE(GetEmailButton()->GetVisible());
  EXPECT_TRUE(GetLockButton()->GetVisible());
  EXPECT_TRUE(GetSignOutButton()->GetVisible());
  EXPECT_TRUE(GetPowerOffButton()->GetVisible());
  EXPECT_TRUE(GetRestartButton()->GetVisible());
}

// The lock button are hidden at the lock screen.
TEST_F(PowerButtonTest, ButtonStatesLockScreen) {
  CreateUserSessions(1);
  BlockUserSession(BLOCKED_BY_LOCK_SCREEN);

  EXPECT_TRUE(GetPowerButton()->GetVisible());

  // No menu buttons are visible before showing the menu.
  EXPECT_FALSE(IsMenuShowing());

  EXPECT_EQ(nullptr, GetEmailButton());
  EXPECT_EQ(nullptr, GetRestartButton());
  EXPECT_EQ(nullptr, GetSignOutButton());
  EXPECT_EQ(nullptr, GetLockButton());
  EXPECT_EQ(nullptr, GetPowerOffButton());

  // Clicks on the power button.
  SimulatePowerButtonPress();

  EXPECT_TRUE(IsMenuShowing());

  EXPECT_EQ(nullptr, GetLockButton());
  EXPECT_TRUE(GetEmailButton()->GetVisible());
  EXPECT_TRUE(GetSignOutButton()->GetVisible());
  EXPECT_TRUE(GetPowerOffButton()->GetVisible());
  EXPECT_TRUE(GetRestartButton()->GetVisible());

  // The multi-profile user chooser is disabled at the lock screen.
  EXPECT_FALSE(GetEmailButton()->GetEnabled());
}

// The lock button is hidden when adding a second multiprofile user.
TEST_F(PowerButtonTest, ButtonStatesAddingUser) {
  CreateUserSessions(1);
  SetUserAddingScreenRunning(true);

  EXPECT_TRUE(GetPowerButton()->GetVisible());

  // No menu buttons are visible before showing the menu.
  EXPECT_FALSE(IsMenuShowing());

  EXPECT_EQ(nullptr, GetEmailButton());
  EXPECT_EQ(nullptr, GetRestartButton());
  EXPECT_EQ(nullptr, GetSignOutButton());
  EXPECT_EQ(nullptr, GetLockButton());
  EXPECT_EQ(nullptr, GetPowerOffButton());

  // Clicks on the power button.
  SimulatePowerButtonPress();

  EXPECT_TRUE(IsMenuShowing());
  EXPECT_EQ(nullptr, GetLockButton());
  EXPECT_TRUE(GetEmailButton()->GetVisible());
  EXPECT_TRUE(GetSignOutButton()->GetVisible());
  EXPECT_TRUE(GetPowerOffButton()->GetVisible());
  EXPECT_TRUE(GetRestartButton()->GetVisible());

  // The multi-profile user chooser is disabled at the lock screen.
  EXPECT_FALSE(GetEmailButton()->GetEnabled());
}

TEST_F(PowerButtonTest, ButtonStatesGuestMode) {
  SimulateGuestLogin();
  SimulatePowerButtonPress();
  EXPECT_TRUE(IsMenuShowing());
  EXPECT_EQ(nullptr, GetEmailButton());
  EXPECT_EQ(nullptr, GetLockButton());
  EXPECT_TRUE(GetSignOutButton()->GetVisible());
  EXPECT_TRUE(GetPowerOffButton()->GetVisible());
  EXPECT_TRUE(GetRestartButton()->GetVisible());
}

TEST_F(PowerButtonTest, EmailIsShownForRegularAccount) {
  SimulateUserLogin("user@gmail.com", user_manager::USER_TYPE_REGULAR);
  SimulatePowerButtonPress();
  EXPECT_TRUE(GetEmailButton()->GetVisible());
  EXPECT_TRUE(GetEmailButton()->GetEnabled());
  EXPECT_EQ(u"user@gmail.com", GetEmailButton()->title());
}

TEST_F(PowerButtonTest, EmailIsShownForChildAccount) {
  SimulateUserLogin("child@gmail.com", user_manager::USER_TYPE_CHILD);
  SimulatePowerButtonPress();
  EXPECT_TRUE(GetEmailButton()->GetVisible());
  // The multi-profile user chooser is disabled for child accounts.
  EXPECT_FALSE(GetEmailButton()->GetEnabled());
  EXPECT_EQ(u"child@gmail.com", GetEmailButton()->title());
}

TEST_F(PowerButtonTest, EmailIsNotShownForPublicAccount) {
  SimulateUserLogin("test@test.com", user_manager::USER_TYPE_PUBLIC_ACCOUNT);
  SimulatePowerButtonPress();
  EXPECT_EQ(nullptr, GetEmailButton());
}

// NOTE: Kiosk user types are not tested because quick settings cannot be
// accessed in kiosk mode.

TEST_F(PowerButtonTest, EmailIsNotShownForActiveDirectory) {
  SimulateUserLogin("test@test.com", user_manager::USER_TYPE_ACTIVE_DIRECTORY);
  SimulatePowerButtonPress();
  EXPECT_EQ(nullptr, GetEmailButton());
}

TEST_F(PowerButtonTest, ClickingEmailShowsUserChooserView) {
  SimulateUserLogin("user@gmail.com", user_manager::USER_TYPE_REGULAR);
  SimulatePowerButtonPress();
  LeftClickOn(GetEmailButton());

  QuickSettingsView* quick_settings_view =
      GetPrimaryUnifiedSystemTray()->bubble()->quick_settings_view();
  EXPECT_TRUE(quick_settings_view->IsDetailedViewShown());
  EXPECT_TRUE(views::IsViewClass<UserChooserView>(
      quick_settings_view->detailed_view()));
}

// Power button's rounded radii should change correctly when switching between
// active/inactive.
TEST_F(PowerButtonTest, ButtonRoundedRadii) {
  CreateUserSessions(1);

  // Sets a LTR locale.
  base::i18n::SetICUDefaultLocale("en_US");

  EXPECT_TRUE(GetPowerButton()->GetVisible());

  EXPECT_EQ(gfx::RoundedCornersF(16, 16, 16, 16),
            GetBackgroundLayer()->rounded_corner_radii());

  // Clicks on the power button.
  SimulatePowerButtonPress();

  EXPECT_EQ(gfx::RoundedCornersF(4, 16, 16, 16),
            GetBackgroundLayer()->rounded_corner_radii());

  // Click on a random button to close the menu.
  LeftClickOn(GetLockButton());

  // Sets a RTL locale.
  base::i18n::SetICUDefaultLocale("ar");

  EXPECT_EQ(gfx::RoundedCornersF(16, 16, 16, 16),
            GetBackgroundLayer()->rounded_corner_radii());

  // Clicks on the power button.
  SimulatePowerButtonPress();

  EXPECT_EQ(gfx::RoundedCornersF(16, 4, 16, 16),
            GetBackgroundLayer()->rounded_corner_radii());
}

}  // namespace ash
