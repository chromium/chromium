// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/session/logout_confirmation_controller.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_window_builder.h"
#include "ash/wm/desks/desks_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/tick_clock.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/session_manager_types.h"
#include "components/user_manager/user_type.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"

namespace ash {

constexpr char kUserEmail[] = "user1@test.com";

class LogoutConfirmationControllerTest : public testing::Test {
 public:
  LogoutConfirmationControllerTest(const LogoutConfirmationControllerTest&) =
      delete;
  LogoutConfirmationControllerTest& operator=(
      const LogoutConfirmationControllerTest&) = delete;

 protected:
  LogoutConfirmationControllerTest();
  ~LogoutConfirmationControllerTest() override;

  void LogOut(LogoutConfirmationController::Source source);

  bool log_out_called_;

  scoped_refptr<base::TestMockTimeTaskRunner> runner_;
  base::SingleThreadTaskRunner::CurrentDefaultHandle
      runner_current_default_handle_;

  LogoutConfirmationController controller_;
};

LogoutConfirmationControllerTest::LogoutConfirmationControllerTest()
    : log_out_called_(false),
      runner_(new base::TestMockTimeTaskRunner),
      runner_current_default_handle_(runner_) {
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
      runner_->NowTicks() + base::Seconds(10),
      LogoutConfirmationController::Source::kShelfExitButton);
  EXPECT_FALSE(log_out_called_);
  runner_->FastForwardBy(base::Seconds(9));
  EXPECT_FALSE(log_out_called_);
  runner_->FastForwardBy(base::Seconds(2));
  EXPECT_TRUE(log_out_called_);
}

// Verifies that when a second request to confirm logout is made and the second
// request's countdown ends before the original request's, the user is logged
// out when the new countdown expires.
TEST_F(LogoutConfirmationControllerTest, DurationShortened) {
  controller_.ConfirmLogout(
      runner_->NowTicks() + base::Seconds(30),
      LogoutConfirmationController::Source::kShelfExitButton);
  EXPECT_FALSE(log_out_called_);
  runner_->FastForwardBy(base::Seconds(9));
  EXPECT_FALSE(log_out_called_);
  controller_.ConfirmLogout(
      runner_->NowTicks() + base::Seconds(10),
      LogoutConfirmationController::Source::kShelfExitButton);
  runner_->FastForwardBy(base::Seconds(9));
  EXPECT_FALSE(log_out_called_);
  runner_->FastForwardBy(base::Seconds(2));
  EXPECT_TRUE(log_out_called_);
}

// Verifies that when a second request to confirm logout is made and the second
// request's countdown ends after the original request's, the user is logged
// out when the original countdown expires.
TEST_F(LogoutConfirmationControllerTest, DurationExtended) {
  controller_.ConfirmLogout(
      runner_->NowTicks() + base::Seconds(10),
      LogoutConfirmationController::Source::kShelfExitButton);
  EXPECT_FALSE(log_out_called_);
  runner_->FastForwardBy(base::Seconds(9));
  EXPECT_FALSE(log_out_called_);
  controller_.ConfirmLogout(
      runner_->NowTicks() + base::Seconds(10),
      LogoutConfirmationController::Source::kShelfExitButton);
  runner_->FastForwardBy(base::Seconds(2));
  EXPECT_TRUE(log_out_called_);
}

// Verifies that when the screen is locked while the countdown is running, the
// user is not logged out, even when the original countdown expires.
TEST_F(LogoutConfirmationControllerTest, Lock) {
  controller_.ConfirmLogout(
      runner_->NowTicks() + base::Seconds(10),
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
      runner_->NowTicks() + base::Seconds(10),
      LogoutConfirmationController::Source::kShelfExitButton);
  EXPECT_FALSE(log_out_called_);
  controller_.OnLogoutConfirmed();
  EXPECT_TRUE(log_out_called_);
}

// Verifies that when the user denies the logout request, the user is not logged
// out, even when the original countdown expires.
TEST_F(LogoutConfirmationControllerTest, UserDenied) {
  controller_.ConfirmLogout(
      runner_->NowTicks() + base::Seconds(10),
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
      runner_->NowTicks() + base::Seconds(10),
      LogoutConfirmationController::Source::kShelfExitButton);
  EXPECT_FALSE(log_out_called_);
  controller_.OnDialogClosed();
  runner_->FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(log_out_called_);

  controller_.ConfirmLogout(
      runner_->NowTicks() + base::Seconds(10),
      LogoutConfirmationController::Source::kShelfExitButton);
  EXPECT_FALSE(log_out_called_);
  runner_->FastForwardBy(base::Seconds(9));
  EXPECT_FALSE(log_out_called_);
  runner_->FastForwardBy(base::Seconds(2));
  EXPECT_TRUE(log_out_called_);
}

class LastWindowClosedTest : public NoSessionAshTestBase {
 public:
  LastWindowClosedTest() = default;

  LastWindowClosedTest(const LastWindowClosedTest&) = delete;
  LastWindowClosedTest& operator=(const LastWindowClosedTest&) = delete;

  ~LastWindowClosedTest() override = default;

  // Simulate a managed guest session (non-demo session) login.
  void StartManagedGuestSession() {
    TestSessionControllerClient* session = GetSessionControllerClient();
    session->Reset();
    session->AddUserSession(kUserEmail, user_manager::UserType::kPublicAccount);
    session->SetSessionState(session_manager::SessionState::ACTIVE);
  }

  // Simulate a demo session signing in.
  void StartDemoSession() {
    GetSessionControllerClient()->SetIsDemoSession();
    // Demo session is implemented as a managed guest session.
    StartManagedGuestSession();
  }

  // PrefService for the managed guest session started by
  // StartManagedGuestSession().
  PrefService* pref_service() {
    return Shell::Get()->session_controller()->GetUserPrefServiceForUser(
        AccountId::FromUserEmail(kUserEmail));
  }
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
  // a managed guest session.
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

TEST_F(LastWindowClosedTest, ManagedGuestSession) {
  LogoutConfirmationController* controller =
      Shell::Get()->logout_confirmation_controller();

  // Dialog is not visible after managed guest session login.
  StartManagedGuestSession();
  EXPECT_FALSE(controller->dialog_for_testing());

  // Opening windows does not show the dialog.
  std::unique_ptr<views::Widget> widget1 =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  std::unique_ptr<views::Widget> widget2 =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  EXPECT_FALSE(controller->dialog_for_testing());

  // Closing the last window shows the dialog.
  widget1.reset();
  EXPECT_FALSE(controller->dialog_for_testing());
  widget2.reset();
  EXPECT_TRUE(controller->dialog_for_testing());
}

TEST_F(LastWindowClosedTest, SuggestLogoutAfterClosingLastWindowPolicy) {
  LogoutConfirmationController* controller =
      Shell::Get()->logout_confirmation_controller();

  // Dialog is not visible after managed guest session login.
  StartManagedGuestSession();
  pref_service()->SetBoolean(prefs::kSuggestLogoutAfterClosingLastWindow,
                             false);
  EXPECT_FALSE(controller->dialog_for_testing());

  // Opening windows does not show the dialog.
  std::unique_ptr<views::Widget> widget1 =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  std::unique_ptr<views::Widget> widget2 =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  EXPECT_FALSE(controller->dialog_for_testing());

  // Closing the last window does not show the dialog because the
  // kSuggestLogoutAfterClosingLastWindow is set to false.
  widget1.reset();
  EXPECT_FALSE(controller->dialog_for_testing());
  widget2.reset();
  EXPECT_FALSE(controller->dialog_for_testing());
}

// Test ARC++ window hierarchy where window minimize, restore and go in/out full
// screen causes a window removing deep inside the top window hierarchy. Actions
// above should no cause logout timer and only closing the last top window
// triggers the logout timer.
TEST_F(LastWindowClosedTest, ManagedGuestSessionComplexHierarchy) {
  LogoutConfirmationController* controller =
      Shell::Get()->logout_confirmation_controller();

  StartManagedGuestSession();
  EXPECT_FALSE(controller->dialog_for_testing());

  std::unique_ptr<aura::Window> window = CreateToplevelTestWindow();
  EXPECT_FALSE(controller->dialog_for_testing());

  std::unique_ptr<aura::Window> window_child =
      ChildTestWindowBuilder(window.get()).Build();
  EXPECT_FALSE(controller->dialog_for_testing());

  window_child.reset();
  EXPECT_FALSE(controller->dialog_for_testing());

  window.reset();
  EXPECT_TRUE(controller->dialog_for_testing());
}

TEST_F(LastWindowClosedTest, AlwaysOnTop) {
  LogoutConfirmationController* controller =
      Shell::Get()->logout_confirmation_controller();
  StartManagedGuestSession();

  // The new widget starts in the default window container.
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);

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
  StartManagedGuestSession();

  // Create two windows in different containers.
  std::unique_ptr<views::Widget> normal_widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  std::unique_ptr<views::Widget> always_on_top_widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
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
  StartManagedGuestSession();

  // Create two displays, each with a window.
  UpdateDisplay("1024x768,800x600");
  std::unique_ptr<aura::Window> window1 =
      ChildTestWindowBuilder(Shell::GetAllRootWindows()[0]->GetChildById(
                                 desks_util::GetActiveDeskContainerId()))
          .Build();
  std::unique_ptr<aura::Window> window2 =
      ChildTestWindowBuilder(Shell::GetAllRootWindows()[1]->GetChildById(
                                 desks_util::GetActiveDeskContainerId()))
          .Build();

  // Closing the last window shows the dialog.
  window1.reset();
  EXPECT_FALSE(controller->dialog_for_testing());
  window2.reset();
  EXPECT_TRUE(controller->dialog_for_testing());
}

}  // namespace ash
