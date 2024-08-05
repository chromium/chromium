// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/quick_settings_footer.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/shell.h"
#include "ash/system/power/adaptive_charging_controller.h"
#include "ash/system/unified/power_button.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/user_manager/user_type.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view_utils.h"
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

  PillButton* GetSignOutButton() { return footer_->sign_out_button_; }

  views::Button* GetSettingsButton() { return footer_->settings_button_; }

  views::View* GetBatteryButton() {
    return footer_->GetViewByID(VIEW_ID_QS_BATTERY_BUTTON);
  }

  PowerButton* GetPowerButton() {
    return static_cast<PowerButton*>(
        footer_->GetViewByID(VIEW_ID_QS_POWER_BUTTON));
  }

  views::View* GetUserAvatar() {
    return footer_->GetViewByID(VIEW_ID_QS_USER_AVATAR_BUTTON);
  }

  void LayoutFooter() { views::test::RunScheduledLayout(footer_); }

 private:
  std::unique_ptr<views::Widget> widget_;

  // Owned by `widget_`.
  raw_ptr<QuickSettingsFooter, DanglingUntriaged> footer_;
};

// Tests that all buttons are with the correct view id, catalog name and UMA
// tracking.
TEST_F(QuickSettingsFooterTest, ButtonNamesAndUMA) {
  CreateUserSessions(2);
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

  ASSERT_TRUE(GetUserAvatar());
  EXPECT_TRUE(GetUserAvatar()->GetVisible());
  EXPECT_EQ(VIEW_ID_QS_USER_AVATAR_BUTTON, GetUserAvatar()->GetID());

  EXPECT_TRUE(GetBatteryButton()->GetVisible());
  EXPECT_EQ(VIEW_ID_QS_BATTERY_BUTTON, GetBatteryButton()->GetID());

  // Test the UMA tracking.
  LeftClickOn(GetPowerButton());

  histogram_tester->ExpectTotalCount("Ash.QuickSettings.Button.Activated",
                                     /*count=*/1);
  histogram_tester->ExpectBucketCount("Ash.QuickSettings.Button.Activated",
                                      QsButtonCatalogName::kPowerButton,
                                      /*expected_count=*/1);

  // Close the power button menu.
  PressAndReleaseKey(ui::VKEY_ESCAPE);

  LeftClickOn(GetBatteryButton());
  histogram_tester->ExpectTotalCount("Ash.QuickSettings.Button.Activated",
                                     /*count=*/2);
  histogram_tester->ExpectBucketCount("Ash.QuickSettings.Button.Activated",
                                      QsButtonCatalogName::kBatteryButton,
                                      /*expected_count=*/1);
}

// Settings button and avatar button are hidden before login.
TEST_F(QuickSettingsFooterTest, ButtonStatesNotLoggedIn) {
  SetUpView();

  EXPECT_FALSE(GetUserAvatar());
  EXPECT_FALSE(GetSettingsButton());
  EXPECT_TRUE(GetPowerButton()->GetVisible());
  EXPECT_TRUE(GetBatteryButton()->GetVisible());
  EXPECT_FALSE(GetSignOutButton());
}

// All buttons are shown after login.
TEST_F(QuickSettingsFooterTest, ButtonStatesLoggedIn) {
  CreateUserSessions(1);
  SetUpView();

  EXPECT_FALSE(GetSignOutButton());

  ASSERT_TRUE(GetSettingsButton());
  EXPECT_TRUE(GetSettingsButton()->GetVisible());

  ASSERT_TRUE(GetPowerButton());
  EXPECT_TRUE(GetPowerButton()->GetVisible());

  ASSERT_TRUE(GetBatteryButton());
  EXPECT_TRUE(GetBatteryButton()->GetVisible());

  // No sign-out button because there is only one account on the device.
  EXPECT_FALSE(GetSignOutButton());

  // No user avatar button because only one user is signed in.
  EXPECT_FALSE(GetUserAvatar());
}

// Settings button is hidden at the lock screen.
TEST_F(QuickSettingsFooterTest, ButtonStatesLockScreen) {
  BlockUserSession(BLOCKED_BY_LOCK_SCREEN);
  SetUpView();

  EXPECT_FALSE(GetSettingsButton());
  ASSERT_TRUE(GetPowerButton());
  EXPECT_TRUE(GetPowerButton()->GetVisible());
  ASSERT_TRUE(GetBatteryButton());
  EXPECT_TRUE(GetBatteryButton()->GetVisible());

  // No user avatar button because we are in the lock screen.
  EXPECT_FALSE(GetUserAvatar());
}

// Settings button and lock button are hidden when adding a second
// multiprofile user.
TEST_F(QuickSettingsFooterTest, ButtonStatesAddingUser) {
  CreateUserSessions(1);
  SetUserAddingScreenRunning(true);
  SetUpView();

  ASSERT_FALSE(GetUserAvatar());
  ASSERT_FALSE(GetSignOutButton());
  EXPECT_EQ(nullptr, GetSettingsButton());
  ASSERT_TRUE(GetPowerButton());
  EXPECT_TRUE(GetPowerButton()->GetVisible());
  ASSERT_TRUE(GetBatteryButton());
  EXPECT_TRUE(GetBatteryButton()->GetVisible());
}

TEST_F(QuickSettingsFooterTest, ButtonStatesGuestMode) {
  SimulateGuestLogin();
  SetUpView();

  ASSERT_TRUE(GetSettingsButton());
  EXPECT_TRUE(GetSettingsButton()->GetVisible());

  ASSERT_TRUE(GetPowerButton());
  EXPECT_TRUE(GetPowerButton()->GetVisible());

  ASSERT_TRUE(GetBatteryButton());
  EXPECT_TRUE(GetBatteryButton()->GetVisible());

  ASSERT_TRUE(GetSignOutButton());
  EXPECT_TRUE(GetSignOutButton()->GetVisible());
  EXPECT_EQ(u"Exit guest", GetSignOutButton()->GetText());
}

TEST_F(QuickSettingsFooterTest, ButtonStatesPublicAccount) {
  SimulateUserLogin("foo@example.com", user_manager::UserType::kPublicAccount);
  SetUpView();

  ASSERT_TRUE(GetSettingsButton());
  EXPECT_TRUE(GetSettingsButton()->GetVisible());

  ASSERT_TRUE(GetPowerButton());
  EXPECT_TRUE(GetPowerButton()->GetVisible());

  ASSERT_TRUE(GetBatteryButton());
  EXPECT_TRUE(GetBatteryButton()->GetVisible());

  ASSERT_TRUE(GetSignOutButton());
  EXPECT_TRUE(GetSignOutButton()->GetVisible());
  EXPECT_EQ(u"Exit session", GetSignOutButton()->GetText());

  EXPECT_FALSE(GetUserAvatar());
}

TEST_F(QuickSettingsFooterTest, SignOutShowsWithMultipleAccounts) {
  GetSessionControllerClient()->set_existing_users_count(2);
  CreateUserSessions(1);
  SetUpView();

  ASSERT_TRUE(GetSignOutButton());
  EXPECT_TRUE(GetSignOutButton()->GetVisible());
  EXPECT_EQ(u"Sign out", GetSignOutButton()->GetText());

  // Although there are two accounts, only one is logged in so do not show the
  // user avatar.
  EXPECT_FALSE(GetUserAvatar());
}

TEST_F(QuickSettingsFooterTest, SignOutButtonRecordsUmaAndSignsOut) {
  // TODO(minch): Re-enable this test.
  if (features::IsForestFeatureEnabled()) {
    GTEST_SKIP() << "Skipping test body for forest feature.";
  }

  GetSessionControllerClient()->set_existing_users_count(2);
  CreateUserSessions(1);
  SetUpView();

  base::HistogramTester histogram_tester;
  LeftClickOn(GetSignOutButton());

  histogram_tester.ExpectTotalCount("Ash.QuickSettings.Button.Activated",
                                    /*expected_count=*/1);
  histogram_tester.ExpectBucketCount("Ash.QuickSettings.Button.Activated",
                                     QsButtonCatalogName::kSignOutButton,
                                     /*expected_count=*/1);

  EXPECT_EQ(1, GetSessionControllerClient()->request_sign_out_count());
}

// Settings button is disabled when kSettingsIconDisabled is set.
TEST_F(QuickSettingsFooterTest, DisableSettingsIconPolicy) {
  GetSessionControllerClient()->AddUserSession(
      "foo@example.com", user_manager::UserType::kRegular);
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  SetUpView();
  EXPECT_EQ(views::Button::STATE_NORMAL, GetSettingsButton()->GetState());

  local_state()->SetBoolean(prefs::kOsSettingsEnabled, false);
  EXPECT_EQ(views::Button::STATE_DISABLED, GetSettingsButton()->GetState());

  local_state()->SetBoolean(prefs::kOsSettingsEnabled, true);
  EXPECT_EQ(views::Button::STATE_NORMAL, GetSettingsButton()->GetState());
}

// Tests different battery states.
TEST_F(QuickSettingsFooterTest, BatteryButtonState) {
  CreateUserSessions(1);
  SetUpView();

  const bool use_smart_charging_ui =
      ash::features::IsAdaptiveChargingEnabled() &&
      Shell::Get()
          ->adaptive_charging_controller()
          ->is_adaptive_delaying_charge();

  if (use_smart_charging_ui) {
    EXPECT_TRUE(views::IsViewClass<QsBatteryIconView>(GetBatteryButton()));
  } else {
    EXPECT_TRUE(views::IsViewClass<QsBatteryLabelView>(GetBatteryButton()));
  }
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
