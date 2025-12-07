// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/session/logout_button_tray.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/session/logout_confirmation_controller.h"
#include "ash/system/session/logout_confirmation_dialog.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/user/login_status.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test_shell_delegate.h"
#include "base/test/metrics/user_action_tester.h"
#include "components/prefs/pref_service.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

constexpr char kUserEmail[] = "user1@test.com";

class LogoutButtonTrayTest : public NoSessionAshTestBase {
 public:
  LogoutButtonTrayTest() = default;

  LogoutButtonTrayTest(const LogoutButtonTrayTest&) = delete;
  LogoutButtonTrayTest& operator=(const LogoutButtonTrayTest&) = delete;

  ~LogoutButtonTrayTest() override = default;

  LogoutButtonTray* tray() {
    return Shell::GetPrimaryRootWindowController()
        ->GetStatusAreaWidget()
        ->logout_button_tray_for_testing();
  }
};

TEST_F(LogoutButtonTrayTest, Visibility) {
  // Button is not visible before login.
  LogoutButtonTray* button = tray();
  ASSERT_TRUE(button);
  EXPECT_FALSE(button->GetVisible());

  // Button is not visible after simulated login.
  SimulateUserLogin({kUserEmail});
  EXPECT_FALSE(button->GetVisible());

  PrefService* pref_service =
      Shell::Get()->session_controller()->GetUserPrefServiceForUser(
          AccountId::FromUserEmail(kUserEmail));

  // Setting the pref makes the button visible.
  pref_service->SetBoolean(prefs::kShowLogoutButtonInTray, true);
  EXPECT_TRUE(button->GetVisible());

  // Locking the screen hides the button.
  GetSessionControllerClient()->LockScreen();
  EXPECT_FALSE(button->GetVisible());

  // Unlocking the screen shows the button.
  GetSessionControllerClient()->UnlockScreen();
  EXPECT_TRUE(button->GetVisible());

  // Resetting the pref hides the button.
  pref_service->SetBoolean(prefs::kShowLogoutButtonInTray, false);
  EXPECT_FALSE(button->GetVisible());
}

TEST_F(LogoutButtonTrayTest, ButtonPressedSignOutImmediately) {
  ASSERT_TRUE(tray());
  views::MdTextButton* const button = tray()->button_for_test();
  TestSessionControllerClient* const session_client =
      GetSessionControllerClient();

  // Sign out immediately when duration is zero.
  auto pref_service = TestPrefServiceProvider::CreateUserPrefServiceSimple();
  pref_service->SetBoolean(prefs::kShowLogoutButtonInTray, true);
  pref_service->SetInteger(prefs::kLogoutDialogDurationMs, 0);
  SimulateUserLogin({kUserEmail}, std::nullopt, std::move(pref_service));

  EXPECT_EQ(0, session_client->request_sign_out_count());
  EXPECT_EQ(0, Shell::Get()
                   ->logout_confirmation_controller()
                   ->confirm_logout_count_for_test());

  LeftClickOn(button);
  session_client->FlushForTest();
  EXPECT_EQ(1, session_client->request_sign_out_count());
  EXPECT_EQ(0, Shell::Get()
                   ->logout_confirmation_controller()
                   ->confirm_logout_count_for_test());
}

TEST_F(LogoutButtonTrayTest, ButtonPressedShowConfirmationDialog) {
  ASSERT_TRUE(tray());
  views::MdTextButton* const button = tray()->button_for_test();
  TestSessionControllerClient* const session_client =
      GetSessionControllerClient();

  // Call |LogoutConfirmationController::ConfirmLogout| when duration is
  // non-zero.
  auto pref_service = TestPrefServiceProvider::CreateUserPrefServiceSimple();
  pref_service->SetBoolean(prefs::kShowLogoutButtonInTray, true);
  pref_service->SetInteger(prefs::kLogoutDialogDurationMs, 1000);
  SimulateUserLogin({kUserEmail}, std::nullopt, std::move(pref_service));

  EXPECT_EQ(0, session_client->request_sign_out_count());
  EXPECT_EQ(0, Shell::Get()
                   ->logout_confirmation_controller()
                   ->confirm_logout_count_for_test());

  LeftClickOn(button);
  session_client->FlushForTest();
  EXPECT_EQ(0, session_client->request_sign_out_count());
  EXPECT_EQ(1, Shell::Get()
                   ->logout_confirmation_controller()
                   ->confirm_logout_count_for_test());

  LogoutConfirmationDialog* dialog =
      Shell::Get()->logout_confirmation_controller()->dialog_for_testing();
  ASSERT_TRUE(dialog);
  dialog->GetWidget()->CloseNow();
  ASSERT_FALSE(
      Shell::Get()->logout_confirmation_controller()->dialog_for_testing());
}

TEST_F(LogoutButtonTrayTest, ButtonPressedInDemoSession) {
  constexpr char kUserAction[] = "DemoMode.ExitFromShelf";
  base::UserActionTester user_action_tester;

  ASSERT_TRUE(tray());
  views::MdTextButton* const button = tray()->button_for_test();
  TestSessionControllerClient* const session_client =
      GetSessionControllerClient();

  // Sign out immediately and record user action when duration is zero
  // and it is demo session.
  auto pref_service = TestPrefServiceProvider::CreateUserPrefServiceSimple();
  pref_service->SetBoolean(prefs::kShowLogoutButtonInTray, true);
  pref_service->SetInteger(prefs::kLogoutDialogDurationMs, 0);
  SimulateUserLogin({kUserEmail}, std::nullopt, std::move(pref_service));
  session_client->SetIsDemoSession();

  LeftClickOn(button);
  session_client->FlushForTest();
  EXPECT_EQ(1, session_client->request_sign_out_count());
  EXPECT_EQ(1, user_action_tester.GetActionCount(kUserAction));
  EXPECT_EQ(0, Shell::Get()
                   ->logout_confirmation_controller()
                   ->confirm_logout_count_for_test());
}

TEST_F(LogoutButtonTrayTest, AccessibleName) {
  SimulateUserLogin({kUserEmail});
  {
    ui::AXNodeData node_data;
    tray()->GetViewAccessibility().GetAccessibleNodeData(&node_data);
    EXPECT_EQ(node_data.GetString16Attribute(ax::mojom::StringAttribute::kName),
              tray()->button_for_test()->GetText());
  }

  tray()->button_for_test()->SetText(u"Testing button text change");

  {
    ui::AXNodeData node_data;
    tray()->GetViewAccessibility().GetAccessibleNodeData(&node_data);
    EXPECT_EQ(node_data.GetString16Attribute(ax::mojom::StringAttribute::kName),
              u"Testing button text change");
  }

  // When the aligntment is kLeft, UpdateLayout will update the button's text to
  // empty string but the accessible name should be updated to a non-empty
  // string, which in turn should update the tray's accessible name.
  tray()->shelf()->SetAlignment(ShelfAlignment::kLeft);
  tray()->UpdateLayout();

  {
    EXPECT_EQ(tray()->button_for_test()->GetText(), std::u16string());
    ui::AXNodeData node_data;
    tray()->GetViewAccessibility().GetAccessibleNodeData(&node_data);
    EXPECT_EQ(node_data.GetString16Attribute(ax::mojom::StringAttribute::kName),
              tray()->GetLoginStatusString());
  }
}

}  // namespace
}  // namespace ash
