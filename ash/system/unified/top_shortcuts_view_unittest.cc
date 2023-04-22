// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/top_shortcuts_view.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/system/unified/collapse_button.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/prefs/pref_registry_simple.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {

// Tests manually control their session visibility.
class TopShortcutsViewTest : public NoSessionAshTestBase {
 public:
  TopShortcutsViewTest() = default;

  TopShortcutsViewTest(const TopShortcutsViewTest&) = delete;
  TopShortcutsViewTest& operator=(const TopShortcutsViewTest&) = delete;

  ~TopShortcutsViewTest() override = default;

  void SetUp() override {
    NoSessionAshTestBase::SetUp();

    model_ = base::MakeRefCounted<UnifiedSystemTrayModel>(nullptr);
    controller_ = std::make_unique<UnifiedSystemTrayController>(model_.get());
    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
  }

  void TearDown() override {
    widget_.reset();
    controller_.reset();
    model_.reset();

    NoSessionAshTestBase::TearDown();
  }

 protected:
  void SetUpView() {
    top_shortcuts_view_ = widget_->SetContentsView(
        std::make_unique<TopShortcutsView>(controller_.get()));
  }

  views::View* GetUserAvatar() {
    return top_shortcuts_view_->GetViewByID(VIEW_ID_QS_USER_AVATAR_BUTTON);
  }

  views::View* GetSignOutButton() {
    return top_shortcuts_view_->GetViewByID(VIEW_ID_QS_SIGN_OUT_BUTTON);
  }

  views::View* GetLockButton() {
    return top_shortcuts_view_->GetViewByID(VIEW_ID_QS_LOCK_BUTTON);
  }

  views::Button* GetSettingsButton() {
    return top_shortcuts_view_->settings_button_;
  }

  views::View* GetPowerButton() {
    return top_shortcuts_view_->GetViewByID(VIEW_ID_QS_POWER_BUTTON);
  }

  views::Button* GetCollapseButton() {
    return top_shortcuts_view_->collapse_button_;
  }

  void LayoutShortcuts() {
    views::test::RunScheduledLayout(top_shortcuts_view_);
  }

 private:
  std::unique_ptr<views::Widget> widget_;
  scoped_refptr<UnifiedSystemTrayModel> model_;
  std::unique_ptr<UnifiedSystemTrayController> controller_;

  // Owned by widget.
  raw_ptr<TopShortcutsView, ExperimentalAsh> top_shortcuts_view_;
};

// All buttons are with the correct view id, catalog name and UMA tracking.
TEST_F(TopShortcutsViewTest, ButtonNames) {
  CreateUserSessions(1);
  SetUpView();

  //`QsButtonCatalogName` should be in synced with the buttton `VIEW_ID_QS*`,
  // which is verified in the `OnChildViewAded` method in each QS parent view.
  //
  // The number of view id should be the number of catalog name -1, since
  // `QsButtonCatalogName` has an extra `kUnknown` type.
  EXPECT_EQ(VIEW_ID_QS_MAX - VIEW_ID_QS_MIN,
            static_cast<int>(QsButtonCatalogName::kMaxValue) - 1);

  // No metrics logged before clicking on any buttons.
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  histogram_tester->ExpectTotalCount("Ash.UnifiedSystemView.Button.Activated",
                                     /*count=*/0);

  // All buttons are visible and with the corresponding id.
  EXPECT_TRUE(GetUserAvatar()->GetVisible());
  EXPECT_EQ(VIEW_ID_QS_USER_AVATAR_BUTTON, GetUserAvatar()->GetID());

  EXPECT_TRUE(GetSignOutButton()->GetVisible());
  EXPECT_EQ(VIEW_ID_QS_SIGN_OUT_BUTTON, GetSignOutButton()->GetID());

  EXPECT_TRUE(GetLockButton()->GetVisible());
  EXPECT_EQ(VIEW_ID_QS_LOCK_BUTTON, GetLockButton()->GetID());

  EXPECT_TRUE(GetSettingsButton()->GetVisible());
  EXPECT_EQ(VIEW_ID_QS_SETTINGS_BUTTON, GetSettingsButton()->GetID());

  EXPECT_TRUE(GetPowerButton()->GetVisible());
  EXPECT_EQ(VIEW_ID_QS_POWER_BUTTON, GetPowerButton()->GetID());

  EXPECT_TRUE(GetCollapseButton()->GetVisible());
  EXPECT_EQ(VIEW_ID_QS_COLLAPSE_BUTTON, GetCollapseButton()->GetID());

  // Pick one button which can process the controller's handle action to test
  // the UMA tracking.
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(
      GetLockButton()->GetBoundsInScreen().CenterPoint());
  event_generator->ClickLeftButton();

  histogram_tester->ExpectTotalCount("Ash.UnifiedSystemView.Button.Activated",
                                     /*count=*/1);
  histogram_tester->ExpectBucketCount("Ash.UnifiedSystemView.Button.Activated",
                                      QsButtonCatalogName::kLockButton,
                                      /*expected_count=*/1);
}

// Settings button and lock button are hidden before login.
TEST_F(TopShortcutsViewTest, ButtonStatesNotLoggedIn) {
  SetUpView();
  EXPECT_EQ(nullptr, GetUserAvatar());
  EXPECT_EQ(nullptr, GetSignOutButton());
  EXPECT_EQ(nullptr, GetLockButton());
  EXPECT_EQ(nullptr, GetSettingsButton());
  EXPECT_TRUE(GetPowerButton()->GetVisible());
  EXPECT_TRUE(GetCollapseButton()->GetVisible());
}

// All buttons are shown after login.
TEST_F(TopShortcutsViewTest, ButtonStatesLoggedIn) {
  CreateUserSessions(1);
  SetUpView();
  EXPECT_TRUE(GetUserAvatar()->GetVisible());
  EXPECT_TRUE(GetSignOutButton()->GetVisible());
  EXPECT_TRUE(GetLockButton()->GetVisible());
  EXPECT_TRUE(GetSettingsButton()->GetVisible());
  EXPECT_TRUE(GetPowerButton()->GetVisible());
  EXPECT_TRUE(GetCollapseButton()->GetVisible());
}

// Settings button and lock button are hidden at the lock screen.
TEST_F(TopShortcutsViewTest, ButtonStatesLockScreen) {
  BlockUserSession(BLOCKED_BY_LOCK_SCREEN);
  SetUpView();
  EXPECT_TRUE(GetUserAvatar()->GetVisible());
  EXPECT_TRUE(GetSignOutButton()->GetVisible());
  EXPECT_EQ(nullptr, GetLockButton());
  EXPECT_EQ(nullptr, GetSettingsButton());
  EXPECT_TRUE(GetPowerButton()->GetVisible());
  EXPECT_TRUE(GetCollapseButton()->GetVisible());
}

// Settings button and lock button are hidden when adding a second
// multiprofile user.
TEST_F(TopShortcutsViewTest, ButtonStatesAddingUser) {
  CreateUserSessions(1);
  SetUserAddingScreenRunning(true);
  SetUpView();
  EXPECT_TRUE(GetUserAvatar()->GetVisible());
  EXPECT_TRUE(GetSignOutButton()->GetVisible());
  EXPECT_EQ(nullptr, GetLockButton());
  EXPECT_EQ(nullptr, GetSettingsButton());
  EXPECT_TRUE(GetPowerButton()->GetVisible());
  EXPECT_TRUE(GetCollapseButton()->GetVisible());
}

// Try to layout buttons before login.
TEST_F(TopShortcutsViewTest, ButtonLayoutNotLoggedIn) {
  SetUpView();
  LayoutShortcuts();
}

// Try to layout buttons after login.
TEST_F(TopShortcutsViewTest, ButtonLayoutLoggedIn) {
  CreateUserSessions(1);
  SetUpView();
  LayoutShortcuts();
}

// Try to layout buttons at the lock screen.
TEST_F(TopShortcutsViewTest, ButtonLayoutLockScreen) {
  BlockUserSession(BLOCKED_BY_LOCK_SCREEN);
  SetUpView();
  LayoutShortcuts();
}

// Try to layout buttons when adding a second multiprofile user.
TEST_F(TopShortcutsViewTest, ButtonLayoutAddingUser) {
  CreateUserSessions(1);
  SetUserAddingScreenRunning(true);
  SetUpView();
  LayoutShortcuts();
}

// Settings button is disabled when kSettingsIconDisabled is set.
TEST_F(TopShortcutsViewTest, DisableSettingsIconPolicy) {
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

}  // namespace ash
