// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/top_shortcuts_view.h"

#include "ash/session/test_session_controller_client.h"
#include "ash/system/unified/collapse_button.h"
#include "ash/system/unified/sign_out_button.h"
#include "ash/system/unified/top_shortcut_button.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ash/test/ash_test_base.h"
#include "base/macros.h"

namespace ash {

// Tests manually control their session visibility.
class TopShortcutsViewTest : public NoSessionAshTestBase {
 public:
  TopShortcutsViewTest() = default;
  ~TopShortcutsViewTest() override = default;

  void SetUp() override {
    NoSessionAshTestBase::SetUp();

    model_ = std::make_unique<UnifiedSystemTrayModel>(nullptr);
    controller_ = std::make_unique<UnifiedSystemTrayController>(model_.get());
  }

  void TearDown() override {
    controller_.reset();
    top_shortcuts_view_.reset();
    model_.reset();
    NoSessionAshTestBase::TearDown();
  }

 protected:
  void SetUpView() {
    top_shortcuts_view_ = std::make_unique<TopShortcutsView>(controller_.get());
  }

  views::View* GetUserAvatar() {
    return top_shortcuts_view_->user_avatar_button_;
  }

  views::Button* GetSignOutButton() {
    return top_shortcuts_view_->sign_out_button_;
  }

  views::Button* GetLockButton() { return top_shortcuts_view_->lock_button_; }

  views::Button* GetSettingsButton() {
    return top_shortcuts_view_->settings_button_;
  }

  views::Button* GetPowerButton() { return top_shortcuts_view_->power_button_; }

  views::Button* GetCollapseButton() {
    return top_shortcuts_view_->collapse_button_;
  }

  void Layout() { top_shortcuts_view_->Layout(); }

 private:
  std::unique_ptr<UnifiedSystemTrayModel> model_;
  std::unique_ptr<UnifiedSystemTrayController> controller_;
  std::unique_ptr<TopShortcutsView> top_shortcuts_view_;

  DISALLOW_COPY_AND_ASSIGN(TopShortcutsViewTest);
};

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

// Settings button and lock button are hidden when adding a supervised user.
TEST_F(TopShortcutsViewTest, ButtonStatesSupervisedUserFlow) {
  // Simulate the add supervised user flow, which is a regular user session but
  // with web UI settings disabled.
  const bool enable_settings = false;
  GetSessionControllerClient()->AddUserSession(
      "foo@example.com", user_manager::USER_TYPE_REGULAR, enable_settings);
  SetUpView();
  EXPECT_EQ(nullptr, GetUserAvatar());
  EXPECT_EQ(nullptr, GetSignOutButton());
  EXPECT_EQ(nullptr, GetLockButton());
  EXPECT_EQ(nullptr, GetSettingsButton());
  EXPECT_TRUE(GetPowerButton()->GetVisible());
  EXPECT_TRUE(GetCollapseButton()->GetVisible());
}

// Try to layout buttons before login.
TEST_F(TopShortcutsViewTest, ButtonLayoutNotLoggedIn) {
  SetUpView();
  Layout();
}

// Try to layout buttons after login.
TEST_F(TopShortcutsViewTest, ButtonLayoutLoggedIn) {
  CreateUserSessions(1);
  SetUpView();
  Layout();
}

// Try to layout buttons at the lock screen.
TEST_F(TopShortcutsViewTest, ButtonLayoutLockScreen) {
  BlockUserSession(BLOCKED_BY_LOCK_SCREEN);
  SetUpView();
  Layout();
}

// Try to layout buttons when adding a second multiprofile user.
TEST_F(TopShortcutsViewTest, ButtonLayoutAddingUser) {
  CreateUserSessions(1);
  SetUserAddingScreenRunning(true);
  SetUpView();
  Layout();
}

// Try to layout buttons when adding a supervised user.
TEST_F(TopShortcutsViewTest, ButtonLayoutSupervisedUserFlow) {
  const bool enable_settings = false;
  GetSessionControllerClient()->AddUserSession(
      "foo@example.com", user_manager::USER_TYPE_REGULAR, enable_settings);
  SetUpView();
  Layout();
}

}  // namespace ash
