// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/user_chooser_detailed_view_controller.h"

#include <memory>

#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/system_tray_test_api.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/overview/overview_controller.h"
#include "components/account_id/account_id.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

AccountId GetActiveUser() {
  return Shell::Get()
      ->session_controller()
      ->GetUserSession(/*user_index=*/0)
      ->user_info.account_id;
}

}  // namespace

class UserChooserDetailedViewControllerTest : public AshTestBase {
 public:
  UserChooserDetailedViewControllerTest() = default;

  UserChooserDetailedViewControllerTest(
      const UserChooserDetailedViewControllerTest&) = delete;
  UserChooserDetailedViewControllerTest& operator=(
      const UserChooserDetailedViewControllerTest&) = delete;

  ~UserChooserDetailedViewControllerTest() override = default;

  // AshTestBase
  void SetUp() override {
    AshTestBase::SetUp();
    tray_test_api_ = std::make_unique<SystemTrayTestApi>();
    disable_animations_ =
        std::make_unique<ui::ScopedAnimationDurationScaleMode>(
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  }

  SystemTrayTestApi* tray_test_api() { return tray_test_api_.get(); }

 private:
  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> disable_animations_;
  std::unique_ptr<SystemTrayTestApi> tray_test_api_;
};

TEST_F(UserChooserDetailedViewControllerTest,
       ShowMultiProfileLoginWithOverview) {
  // Enter overview mode.
  EnterOverview();
  ASSERT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  // Show system tray.
  tray_test_api()->ShowBubble();
  ASSERT_TRUE(tray_test_api()->IsTrayBubbleOpen());

  // Click on user avatar button to start user adding.
  ASSERT_TRUE(
      tray_test_api()->IsBubbleViewVisible(VIEW_ID_QS_USER_AVATAR_BUTTON,
                                           /*open_tray=*/false));
  tray_test_api()->ClickBubbleView(VIEW_ID_QS_USER_AVATAR_BUTTON);

  // Click on add user button to show multi profile login window.
  ASSERT_TRUE(tray_test_api()->IsBubbleViewVisible(VIEW_ID_ADD_USER_BUTTON,
                                                   /*open_tray=*/false));
  tray_test_api()->ClickBubbleView(VIEW_ID_ADD_USER_BUTTON);
}

TEST_F(UserChooserDetailedViewControllerTest, SwitchUserWithOverview) {
  // Add a secondary user.
  const AccountId secondary_user =
      AccountId::FromUserEmail("secondary@gmail.com");
  GetSessionControllerClient()->AddUserSession(secondary_user.GetUserEmail());
  ASSERT_NE(GetActiveUser(), secondary_user);

  // Create an activatable widget.
  std::unique_ptr<views::Widget> widget = CreateTestWidget();

  // Enter overview mode.
  EnterOverview();
  ASSERT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  // Show system tray.
  tray_test_api()->ShowBubble();
  ASSERT_TRUE(tray_test_api()->IsTrayBubbleOpen());

  // Click on user avatar button to select user.
  ASSERT_TRUE(
      tray_test_api()->IsBubbleViewVisible(VIEW_ID_QS_USER_AVATAR_BUTTON,
                                           /*open_tray=*/false));
  tray_test_api()->ClickBubbleView(VIEW_ID_QS_USER_AVATAR_BUTTON);

  const int secondary_user_button_id = VIEW_ID_USER_ITEM_BUTTON_START + 1;
  ASSERT_TRUE(tray_test_api()->IsBubbleViewVisible(secondary_user_button_id,
                                                   /*open_tray=*/false));
  tray_test_api()->ClickBubbleView(secondary_user_button_id);

  // Active user is switched.
  EXPECT_EQ(GetActiveUser(), secondary_user);
}

TEST_F(UserChooserDetailedViewControllerTest,
       MultiProfileLoginDisabledForFamilyLinkUsers) {
  EXPECT_TRUE(UserChooserDetailedViewController::IsUserChooserEnabled());

  GetSessionControllerClient()->Reset();

  // Log in as a child user.
  SimulateUserLogin("child@gmail.com", user_manager::USER_TYPE_CHILD);

  EXPECT_FALSE(UserChooserDetailedViewController::IsUserChooserEnabled());
}

}  // namespace ash
