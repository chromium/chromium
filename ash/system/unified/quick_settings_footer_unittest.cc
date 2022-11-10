// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/quick_settings_footer.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/system/unified/power_button.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {

// Tests for `QuickSettingsFooter`, which is inited with no user session to test
// from the non-logged-in state to the logged-in state.
class QuickSettingsFooterTest : public NoSessionAshTestBase {
 public:
  QuickSettingsFooterTest() = default;
  QuickSettingsFooterTest(const QuickSettingsFooterTest&) = delete;
  QuickSettingsFooterTest& operator=(const QuickSettingsFooterTest&) = delete;
  ~QuickSettingsFooterTest() override = default;

  void SetUp() override {
    feature_list_.InitWithFeatures(
        {features::kQsRevamp, features::kQsRevampWip}, {});
    NoSessionAshTestBase::SetUp();
    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
  }

  void TearDown() override {
    widget_.reset();
    NoSessionAshTestBase::TearDown();
  }

 protected:
  void SetUpView() {
    GetPrimaryUnifiedSystemTray()->ShowBubble();
    footer_ = widget_->SetContentsView(std::make_unique<QuickSettingsFooter>(
        GetPrimaryUnifiedSystemTray()
            ->bubble()
            ->unified_system_tray_controller()));
  }

  views::MenuItemView* GetMenuView() {
    return GetPowerButton()->GetMenuViewForTesting();
  }

  bool IsMenuShowing() { return GetPowerButton()->IsMenuShowing(); }

  views::View* GetSignOutButton() {
    if (!IsMenuShowing())
      return nullptr;

    return GetMenuView()->GetMenuItemByID(VIEW_ID_QS_POWER_SIGNOUT_MENU_BUTTON);
  }

  views::View* GetLockButton() {
    if (!IsMenuShowing())
      return nullptr;

    return GetMenuView()->GetMenuItemByID(VIEW_ID_QS_POWER_LOCK_MENU_BUTTON);
  }

  views::View* GetRestartButton() {
    if (!IsMenuShowing())
      return nullptr;

    return GetMenuView()->GetMenuItemByID(VIEW_ID_QS_POWER_RESTART_MENU_BUTTON);
  }

  views::View* GetPowerOffButton() {
    if (!IsMenuShowing())
      return nullptr;

    return GetMenuView()->GetMenuItemByID(VIEW_ID_QS_POWER_OFF_MENU_BUTTON);
  }

  views::Button* GetSettingsButton() { return footer_->settings_button_; }

  views::View* GetBatteryButton() {
    return footer_->GetViewByID(VIEW_ID_QS_BATTERY_BUTTON);
  }

  PowerButton* GetPowerButton() {
    return static_cast<PowerButton*>(
        footer_->GetViewByID(VIEW_ID_QS_POWER_BUTTON));
  }

  void LayoutFooter() { views::test::RunScheduledLayout(footer_); }

 private:
  std::unique_ptr<views::Widget> widget_;

  // Owned by `widget_`.
  QuickSettingsFooter* footer_;

  base::test::ScopedFeatureList feature_list_;
};

// Tests that all buttons are with the correct view id, catalog name and UMA
// tracking.
TEST_F(QuickSettingsFooterTest, ButtonNamesAndUMA) {
  CreateUserSessions(1);
  SetUpView();

  // The number of view id should be the number of catalog name -1, since
  // `QsButtonCatalogName` has an extra `kUnknown` type.
  EXPECT_EQ(VIEW_ID_QS_MAX - VIEW_ID_QS_MIN,
            static_cast<int>(QsButtonCatalogName::kMaxValue) - 1);

  // No metrics logged before clicking on any buttons.
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  histogram_tester->ExpectTotalCount("Ash.QuickSettings.Button.Activated",
                                     /*count=*/0);

  // All buttons are visible and with the corresponding id.
  EXPECT_TRUE(GetSettingsButton()->GetVisible());
  EXPECT_EQ(VIEW_ID_QS_SETTINGS_BUTTON, GetSettingsButton()->GetID());

  EXPECT_TRUE(GetPowerButton()->GetVisible());
  EXPECT_EQ(VIEW_ID_QS_POWER_BUTTON, GetPowerButton()->GetID());

  EXPECT_TRUE(GetBatteryButton()->GetVisible());
  EXPECT_EQ(VIEW_ID_QS_BATTERY_BUTTON, GetBatteryButton()->GetID());

  // No menu buttons are visible before showing the menu.
  EXPECT_FALSE(IsMenuShowing());
  EXPECT_EQ(nullptr, GetRestartButton());
  EXPECT_EQ(nullptr, GetSignOutButton());
  EXPECT_EQ(nullptr, GetLockButton());
  EXPECT_EQ(nullptr, GetPowerOffButton());

  // Test the UMA tracking.
  LeftClickOn(GetPowerButton());

  histogram_tester->ExpectTotalCount("Ash.QuickSettings.Button.Activated",
                                     /*count=*/1);
  histogram_tester->ExpectBucketCount("Ash.QuickSettings.Button.Activated",
                                      QsButtonCatalogName::kPowerButton,
                                      /*expected_count=*/1);

  EXPECT_TRUE(IsMenuShowing());

  // Show all buttons in the menu.
  EXPECT_TRUE(GetLockButton()->GetVisible());
  EXPECT_TRUE(GetSignOutButton()->GetVisible());
  EXPECT_TRUE(GetPowerOffButton()->GetVisible());
  EXPECT_TRUE(GetRestartButton()->GetVisible());

  // Close the power button menu.
  PressAndReleaseKey(ui::VKEY_ESCAPE);

  LeftClickOn(GetBatteryButton());
  histogram_tester->ExpectTotalCount("Ash.QuickSettings.Button.Activated",
                                     /*count=*/2);
  histogram_tester->ExpectBucketCount("Ash.QuickSettings.Button.Activated",
                                      QsButtonCatalogName::kBatteryButton,
                                      /*expected_count=*/1);
}

// Settings button is hidden before login.
TEST_F(QuickSettingsFooterTest, ButtonStatesNotLoggedIn) {
  SetUpView();

  EXPECT_EQ(nullptr, GetSettingsButton());
  EXPECT_TRUE(GetPowerButton()->GetVisible());
  EXPECT_TRUE(GetBatteryButton()->GetVisible());
}

// All buttons are shown after login.
TEST_F(QuickSettingsFooterTest, ButtonStatesLoggedIn) {
  CreateUserSessions(1);
  SetUpView();

  EXPECT_TRUE(GetSettingsButton()->GetVisible());
  EXPECT_TRUE(GetPowerButton()->GetVisible());
  EXPECT_TRUE(GetBatteryButton()->GetVisible());

  // No menu buttons are visible before showing the menu.
  EXPECT_FALSE(IsMenuShowing());
  EXPECT_EQ(nullptr, GetRestartButton());
  EXPECT_EQ(nullptr, GetSignOutButton());
  EXPECT_EQ(nullptr, GetLockButton());
  EXPECT_EQ(nullptr, GetPowerOffButton());

  // Clicks on the power button.
  LeftClickOn(GetPowerButton());

  EXPECT_TRUE(IsMenuShowing());

  // Show all buttons in the menu.
  EXPECT_TRUE(GetLockButton()->GetVisible());
  EXPECT_TRUE(GetSignOutButton()->GetVisible());
  EXPECT_TRUE(GetPowerOffButton()->GetVisible());
  EXPECT_TRUE(GetRestartButton()->GetVisible());
}

// Settings button is hidden at the lock screen.
TEST_F(QuickSettingsFooterTest, ButtonStatesLockScreen) {
  BlockUserSession(BLOCKED_BY_LOCK_SCREEN);
  SetUpView();

  EXPECT_EQ(nullptr, GetSettingsButton());
  EXPECT_TRUE(GetPowerButton()->GetVisible());
  EXPECT_TRUE(GetBatteryButton()->GetVisible());
}

// Settings button and lock button are hidden when adding a second
// multiprofile user.
TEST_F(QuickSettingsFooterTest, ButtonStatesAddingUser) {
  CreateUserSessions(1);
  SetUserAddingScreenRunning(true);
  SetUpView();

  EXPECT_EQ(nullptr, GetSettingsButton());
  EXPECT_TRUE(GetPowerButton()->GetVisible());
  EXPECT_TRUE(GetBatteryButton()->GetVisible());
}

// Settings button is disabled when kSettingsIconDisabled is set.
TEST_F(QuickSettingsFooterTest, DisableSettingsIconPolicy) {
  GetSessionControllerClient()->AddUserSession("foo@example.com",
                                               user_manager::USER_TYPE_REGULAR);
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  SetUpView();
  EXPECT_EQ(views::Button::STATE_NORMAL, GetSettingsButton()->GetState());

  local_state()->SetBoolean(prefs::kOsSettingsEnabled, false);
  EXPECT_EQ(views::Button::STATE_DISABLED, GetSettingsButton()->GetState());

  local_state()->SetBoolean(prefs::kOsSettingsEnabled, true);
  EXPECT_EQ(views::Button::STATE_NORMAL, GetSettingsButton()->GetState());
}

// The following tests will ensure that the entire Widget root view is properly
// laid out. The `LayoutFooter()` method will call
// `Widget::LayoutRootViewIfNecessary()`.
//
// Try to layout buttons before login.
TEST_F(QuickSettingsFooterTest, ButtonLayoutNotLoggedIn) {
  SetUpView();
  LayoutFooter();
}

// Try to layout buttons after login.
TEST_F(QuickSettingsFooterTest, ButtonLayoutLoggedIn) {
  CreateUserSessions(1);
  SetUpView();
  LayoutFooter();
}

// Try to layout buttons at the lock screen.
TEST_F(QuickSettingsFooterTest, ButtonLayoutLockScreen) {
  BlockUserSession(BLOCKED_BY_LOCK_SCREEN);
  SetUpView();
  LayoutFooter();
}

// Try to layout buttons when adding a second multiprofile user.
TEST_F(QuickSettingsFooterTest, ButtonLayoutAddingUser) {
  CreateUserSessions(1);
  SetUserAddingScreenRunning(true);
  SetUpView();
  LayoutFooter();
}

}  // namespace ash
