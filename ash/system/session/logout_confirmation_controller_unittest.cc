// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/session/logout_confirmation_controller.h"

#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/desks_util.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/tick_clock.h"
#include "components/session_manager/session_manager_types.h"
#include "components/user_manager/user_type.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"

namespace ash {

class LogoutConfirmationControllerTest : public testing::Test {
 protected:
  LogoutConfirmationControllerTest();
  ~LogoutConfirmationControllerTest() override;

  void LogOut(LogoutConfirmationController::Source source);

  bool log_out_called_;

  scoped_refptr<base::TestMockTimeTaskRunner> runner_;
  base::ThreadTaskRunnerHandle runner_handle_;

  LogoutConfirmationController controller_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LogoutConfirmationControllerTest);
};

LogoutConfirmationControllerTest::LogoutConfirmationControllerTest()
    : log_out_called_(false),
      runner_(new base::TestMockTimeTaskRunner),
      runner_handle_(runner_) {
  controller_.SetClockForTesting(runner_->GetMockTickClock());
  controller_.SetLogoutCallbackForTesting(base::BindRepeating(
      &LogoutConfirmationControllerTest::LogOut, base::Unretained(this)));
}

LogoutConfirmationControllerTest::~LogoutConfirmationControllerTest() = default;

void LogoutConfirmationControllerTest::LogOut(
    LogoutConfirmationController::Source source) {
  log_out_called_ = true;
}

// Verifies that the user is logged out immediately if logout confirmation with
// a zero-length countdown is requested.
TEST_F(LogoutConfirmationControllerTest, ZeroDuration) {
  controller_.ConfirmLogout(
      runner_->NowTicks(),
      LogoutConfirmationController::Source::kShelfExitButton);
  EXPECT_FALSE(log_out_called_);
  runner_->FastForwardBy(base::TimeDelta());
  EXPECT_TRUE(log_out_called_);
}

// Verifies that the user is logged out when the countdown expires.
TEST_F(LogoutConfirmationControllerTest, DurationExpired) {
  controller_.ConfirmLogout(
      runner_->NowTicks() + base::TimeDelta::FromSeconds(10),
      LogoutConfirmationController::Source::kShelfExitButton);
  EXPECT_FALSE(log_out_called_);
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(9));
  EXPECT_FALSE(log_out_called_);
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(2));
  EXPECT_TRUE(log_out_called_);
}

// Verifies that when a second request to confirm logout is made and the second
// request's countdown ends before the original request's, the user is logged
// out when the new countdown expires.
TEST_F(LogoutConfirmationControllerTest, DurationShortened) {
  controller_.ConfirmLogout(
      runner_->NowTicks() + base::TimeDelta::FromSeconds(30),
      LogoutConfirmationController::Source::kShelfExitButton);
  EXPECT_FALSE(log_out_called_);
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(9));
  EXPECT_FALSE(log_out_called_);
  controller_.ConfirmLogout(
      runner_->NowTicks() + base::TimeDelta::FromSeconds(10),
      LogoutConfirmationController::Source::kShelfExitButton);
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(9));
  EXPECT_FALSE(log_out_called_);
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(2));
  EXPECT_TRUE(log_out_called_);
}

// Verifies that when a second request to confirm logout is made and the second
// request's countdown ends after the original request's, the user is logged
// out when the original countdown expires.
TEST_F(LogoutConfirmationControllerTest, DurationExtended) {
  controller_.ConfirmLogout(
      runner_->NowTicks() + base::TimeDelta::FromSeconds(10),
      LogoutConfirmationController::Source::kShelfExitButton);
  EXPECT_FALSE(log_out_called_);
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(9));
  EXPECT_FALSE(log_out_called_);
  controller_.ConfirmLogout(
      runner_->NowTicks() + base::TimeDelta::FromSeconds(10),
      LogoutConfirmationController::Source::kShelfExitButton);
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(2));
  EXPECT_TRUE(log_out_called_);
}

// Verifies that when the screen is locked while the countdown is running, the
// user is not logged out, even when the original countdown expires.
TEST_F(LogoutConfirmationControllerTest, Lock) {
  controller_.ConfirmLogout(
      runner_->NowTicks() + base::TimeDelta::FromSeconds(10),
      LogoutConfirmationController::Source::kShelfExitButton);
  EXPECT_FALSE(log_out_called_);
  controller_.OnLockStateChanged(true);
  runner_->FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(log_out_called_);
}

// Verifies that when the user confirms the logout request, the user is logged
// out immediately.
TEST_F(LogoutConfirmationControllerTest, UserAccepted) {
  controller_.ConfirmLogout(
      runner_->NowTicks() + base::TimeDelta::FromSeconds(10),
      LogoutConfirmationController::Source::kShelfExitButton);
  EXPECT_FALSE(log_out_called_);
  controller_.OnLogoutConfirmed();
  EXPECT_TRUE(log_out_called_);
}

// Verifies that when the user denies the logout request, the user is not logged
// out, even when the original countdown expires.
TEST_F(LogoutConfirmationControllerTest, UserDenied) {
  controller_.ConfirmLogout(
      runner_->NowTicks() + base::TimeDelta::FromSeconds(10),
      LogoutConfirmationController::Source::kShelfExitButton);
  EXPECT_FALSE(log_out_called_);
  controller_.OnDialogClosed();
  runner_->FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(log_out_called_);
}

// Verifies that after the user has denied a logout request, a subsequent logout
// request is handled correctly and the user is logged out when the countdown
// expires.
TEST_F(LogoutConfirmationControllerTest, DurationExpiredAfterDeniedRequest) {
  controller_.ConfirmLogout(
      runner_->NowTicks() + base::TimeDelta::FromSeconds(10),
      LogoutConfirmationController::Source::kShelfExitButton);
  EXPECT_FALSE(log_out_called_);
  controller_.OnDialogClosed();
  runner_->FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(log_out_called_);

  controller_.ConfirmLogout(
      runner_->NowTicks() + base::TimeDelta::FromSeconds(10),
      LogoutConfirmationController::Source::kShelfExitButton);
  EXPECT_FALSE(log_out_called_);
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(9));
  EXPECT_FALSE(log_out_called_);
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(2));
  EXPECT_TRUE(log_out_called_);
}

class LastWindowClosedTest : public NoSessionAshTestBase {
 public:
  LastWindowClosedTest() = default;
  ~LastWindowClosedTest() override = default;

  // Simulate a public account (non-demo session) signing in.
  void StartPublicAccountSession() {
    TestSessionControllerClient* session = GetSessionControllerClient();
    session->Reset();
    session->AddUserSession("user1@test.com",
                            user_manager::USER_TYPE_PUBLIC_ACCOUNT);
    session->SetSessionState(session_manager::SessionState::ACTIVE);
  }

  // Simulate a demo session signing in.
  void StartDemoSession() {
    GetSessionControllerClient()->SetIsDemoSession();
    // Demo session is implemented as a public session.
    StartPublicAccountSession();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LastWindowClosedTest);
};

TEST_F(LastWindowClosedTest, RegularSession) {
  // Dialog is not visible at startup.
  LogoutConfirmationController* controller =
      Shell::Get()->logout_confirmation_controller();
  EXPECT_FALSE(controller->dialog_for_testing());

  // Dialog is not visible after login.
  CreateUserSessions(1);
  EXPECT_FALSE(controller->dialog_for_testing());

  // Creating and closing a window does not show the dialog because this is not
  // a public account session.
  std::unique_ptr<aura::Window> window = CreateToplevelTestWindow();
  EXPECT_FALSE(controller->dialog_for_testing());
  window.reset();
  EXPECT_FALSE(controller->dialog_for_testing());
}

TEST_F(LastWindowClosedTest, DemoSession) {
  // Dialog is not visible at startup.
  LogoutConfirmationController* controller =
      Shell::Get()->logout_confirmation_controller();
  EXPECT_FALSE(controller->dialog_for_testing());

  // Dialog is not visible after demo session starts.
  StartDemoSession();
  EXPECT_FALSE(controller->dialog_for_testing());

  // Creating and closing a window does not show the dialog.
  std::unique_ptr<aura::Window> window = CreateToplevelTestWindow();
  EXPECT_FALSE(controller->dialog_for_testing());
  window.reset();
  EXPECT_FALSE(controller->dialog_for_testing());
}

TEST_F(LastWindowClosedTest, PublicSession) {
  LogoutConfirmationController* controller =
      Shell::Get()->logout_confirmation_controller();

  // Dialog is not visible after public account login.
  StartPublicAccountSession();
  EXPECT_FALSE(controller->dialog_for_testing());

  // Opening windows does not show the dialog.
  std::unique_ptr<views::Widget> widget1 = CreateTestWidget();
  std::unique_ptr<views::Widget> widget2 = CreateTestWidget();
  EXPECT_FALSE(controller->dialog_for_testing());

  // Closing the last window shows the dialog.
  widget1.reset();
  EXPECT_FALSE(controller->dialog_for_testing());
  widget2.reset();
  EXPECT_TRUE(controller->dialog_for_testing());
}

// Test ARC++ window hierarchy where window minimize, restore and go in/out full
// screen causes a window removing deep inside the top window hierarchy. Actions
// above should no cause logout timer and only closing the last top window
// triggers the logout timer.
TEST_F(LastWindowClosedTest, PublicSessionComplexHierarchy) {
  LogoutConfirmationController* controller =
      Shell::Get()->logout_confirmation_controller();

  StartPublicAccountSession();
  EXPECT_FALSE(controller->dialog_for_testing());

  std::unique_ptr<aura::Window> window = CreateToplevelTestWindow();
  EXPECT_FALSE(controller->dialog_for_testing());

  std::unique_ptr<aura::Window> window_child = CreateChildWindow(window.get());
  EXPECT_FALSE(controller->dialog_for_testing());

  window_child.reset();
  EXPECT_FALSE(controller->dialog_for_testing());

  window.reset();
  EXPECT_TRUE(controller->dialog_for_testing());
}

TEST_F(LastWindowClosedTest, AlwaysOnTop) {
  LogoutConfirmationController* controller =
      Shell::Get()->logout_confirmation_controller();
  StartPublicAccountSession();

  // The new widget starts in the default window container.
  std::unique_ptr<views::Widget> widget = CreateTestWidget();

  // Moving the widget to the always-on-top container does not trigger the
  // dialog because the window didn't close.
  widget->SetZOrderLevel(ui::ZOrderLevel::kFloatingWindow);
  EXPECT_FALSE(controller->dialog_for_testing());

  // Closing the window triggers the dialog.
  widget.reset();
  EXPECT_TRUE(controller->dialog_for_testing());
}

TEST_F(LastWindowClosedTest, MultipleContainers) {
  LogoutConfirmationController* controller =
      Shell::Get()->logout_confirmation_controller();
  StartPublicAccountSession();

  // Create two windows in different containers.
  std::unique_ptr<views::Widget> normal_widget = CreateTestWidget();
  std::unique_ptr<views::Widget> always_on_top_widget = CreateTestWidget();
  always_on_top_widget->SetZOrderLevel(ui::ZOrderLevel::kFloatingWindow);

  // Closing the last window shows the dialog.
  always_on_top_widget.reset();
  EXPECT_FALSE(controller->dialog_for_testing());
  normal_widget.reset();
  EXPECT_TRUE(controller->dialog_for_testing());
}

TEST_F(LastWindowClosedTest, MultipleDisplays) {
  LogoutConfirmationController* controller =
      Shell::Get()->logout_confirmation_controller();
  StartPublicAccountSession();

  // Create two displays, each with a window.
  UpdateDisplay("1024x768,800x600");
  std::unique_ptr<aura::Window> window1 =
      CreateChildWindow(Shell::GetAllRootWindows()[0]->GetChildById(
          desks_util::GetActiveDeskContainerId()));
  std::unique_ptr<aura::Window> window2 =
      CreateChildWindow(Shell::GetAllRootWindows()[1]->GetChildById(
          desks_util::GetActiveDeskContainerId()));

  // Closing the last window shows the dialog.
  window1.reset();
  EXPECT_FALSE(controller->dialog_for_testing());
  window2.reset();
  EXPECT_TRUE(controller->dialog_for_testing());
}

}  // namespace ash
