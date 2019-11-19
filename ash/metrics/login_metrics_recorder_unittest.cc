// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/login_metrics_recorder.h"

#include <memory>
#include <string>

#include "ash/login/login_screen_controller.h"
#include "ash/login/mock_login_screen_client.h"
#include "ash/login/ui/fake_login_detachable_base_model.h"
#include "ash/login/ui/lock_contents_view.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/login/ui/login_auth_user_view.h"
#include "ash/login/ui/login_big_user_view.h"
#include "ash/login/ui/login_test_base.h"
#include "ash/login/ui/login_test_utils.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

constexpr char kNumAttemptTilSuccessHistogramName[] =
    "Ash.Login.Lock.NumPasswordAttempts.UntilSuccess";
constexpr char kNumAttemptTilFailureHistogramName[] =
    "Ash.Login.Lock.NumPasswordAttempts.UntilFailure";
constexpr char kUserClicksOnLockHistogramName[] = "Ash.Login.Lock.UserClicks";
constexpr char kUserClicksOnLoginHistogramName[] = "Ash.Login.Login.UserClicks";

// Test fixture for the LoginMetricsRecorder class.
class LoginMetricsRecorderTest : public LoginTestBase {
 public:
  LoginMetricsRecorderTest() = default;
  ~LoginMetricsRecorderTest() override = default;

  // LoginTestBase:
  void SetUp() override {
    LoginTestBase::SetUp();
    histogram_tester_.reset(new base::HistogramTester());
  }

 protected:
  void EnableTabletMode(bool enable) {
    Shell::Get()->tablet_mode_controller()->SetEnabledForTest(enable);
  }

  LoginMetricsRecorder* metrics_recorder() {
    return Shell::Get()->metrics()->login_metrics_recorder();
  }

  void ExpectBucketCount(const std::string& name,
                         LoginMetricsRecorder::LockScreenUserClickTarget target,
                         int count) {
    histogram_tester_->ExpectBucketCount(name, static_cast<int>(target), count);
  }

  void ExpectBucketCount(
      const std::string& name,
      LoginMetricsRecorder::LoginScreenUserClickTarget target,
      int count) {
    histogram_tester_->ExpectBucketCount(name, static_cast<int>(target), count);
  }

  // Used to verify recorded data.
  std::unique_ptr<base::HistogramTester> histogram_tester_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginMetricsRecorderTest);
};

}  // namespace

// Verifies that click on the note action button is recorded correctly.
TEST_F(LoginMetricsRecorderTest, NoteActionButtonClick) {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);

  auto* contents = new LockContentsView(
      mojom::TrayActionState::kAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetUserCount(1);
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);

  LockContentsView::TestApi test_api(contents);
  EXPECT_TRUE(test_api.note_action()->GetVisible());

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(
      test_api.note_action()->GetBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();

  histogram_tester_->ExpectTotalCount(kUserClicksOnLockHistogramName, 1);
  histogram_tester_->ExpectBucketCount(
      kUserClicksOnLockHistogramName,
      static_cast<int>(LoginMetricsRecorder::LockScreenUserClickTarget::
                           kLockScreenNoteActionButton),
      1);
}

// Verifies that number of auth attempts are recorded correctly.
TEST_F(LoginMetricsRecorderTest, RecordNumLoginAttempts) {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);

  metrics_recorder()->RecordNumLoginAttempts(5, true /*success*/);
  histogram_tester_->ExpectTotalCount(kNumAttemptTilSuccessHistogramName, 1);
  histogram_tester_->ExpectBucketCount(kNumAttemptTilSuccessHistogramName, 5,
                                       1);

  metrics_recorder()->RecordNumLoginAttempts(7, false /*success*/);
  histogram_tester_->ExpectTotalCount(kNumAttemptTilSuccessHistogramName, 1);
  histogram_tester_->ExpectBucketCount(kNumAttemptTilSuccessHistogramName, 5,
                                       1);
  histogram_tester_->ExpectTotalCount(kNumAttemptTilFailureHistogramName, 1);
  histogram_tester_->ExpectBucketCount(kNumAttemptTilFailureHistogramName, 7,
                                       1);
}

// Verifies that the number of auth attempts at sign in is record successfully.
TEST_F(LoginMetricsRecorderTest, RecordNumSignInAttempts) {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);

  metrics_recorder()->RecordNumLoginAttempts(5, true /*success*/);
  histogram_tester_->ExpectTotalCount(kNumAttemptTilSuccessHistogramName, 1);
  histogram_tester_->ExpectBucketCount(kNumAttemptTilSuccessHistogramName, 5,
                                       1);

  metrics_recorder()->RecordNumLoginAttempts(7, false /*success*/);
  histogram_tester_->ExpectTotalCount(kNumAttemptTilSuccessHistogramName, 1);
  histogram_tester_->ExpectBucketCount(kNumAttemptTilSuccessHistogramName, 5,
                                       1);
  histogram_tester_->ExpectTotalCount(kNumAttemptTilFailureHistogramName, 1);
  histogram_tester_->ExpectBucketCount(kNumAttemptTilFailureHistogramName, 7,
                                       1);
}

// Verifies that user click events on the lock screen is recorded correctly.
TEST_F(LoginMetricsRecorderTest, RecordUserClickEventOnLockScreen) {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);

  // Clicks on shelf buttons visible during lock should be recorded.
  histogram_tester_->ExpectTotalCount(kUserClicksOnLockHistogramName, 0);
  metrics_recorder()->RecordUserShelfButtonClick(
      LoginMetricsRecorder::ShelfButtonClickTarget::kShutDownButton);
  histogram_tester_->ExpectTotalCount(kUserClicksOnLockHistogramName, 1);
  ExpectBucketCount(
      kUserClicksOnLockHistogramName,
      LoginMetricsRecorder::LockScreenUserClickTarget::kShutDownButton, 1);
  histogram_tester_->ExpectTotalCount(kUserClicksOnLoginHistogramName, 0);

  metrics_recorder()->RecordUserShelfButtonClick(
      LoginMetricsRecorder::ShelfButtonClickTarget::kRestartButton);
  histogram_tester_->ExpectTotalCount(kUserClicksOnLockHistogramName, 2);
  ExpectBucketCount(
      kUserClicksOnLockHistogramName,
      LoginMetricsRecorder::LockScreenUserClickTarget::kRestartButton, 1);
  histogram_tester_->ExpectTotalCount(kUserClicksOnLoginHistogramName, 0);

  metrics_recorder()->RecordUserShelfButtonClick(
      LoginMetricsRecorder::ShelfButtonClickTarget::kSignOutButton);
  histogram_tester_->ExpectTotalCount(kUserClicksOnLockHistogramName, 3);
  ExpectBucketCount(
      kUserClicksOnLockHistogramName,
      LoginMetricsRecorder::LockScreenUserClickTarget::kSignOutButton, 1);
  histogram_tester_->ExpectTotalCount(kUserClicksOnLoginHistogramName, 0);

  metrics_recorder()->RecordUserShelfButtonClick(
      LoginMetricsRecorder::ShelfButtonClickTarget::kCloseNoteButton);
  histogram_tester_->ExpectTotalCount(kUserClicksOnLockHistogramName, 4);
  ExpectBucketCount(
      kUserClicksOnLockHistogramName,
      LoginMetricsRecorder::LockScreenUserClickTarget::kCloseNoteButton, 1);
  histogram_tester_->ExpectTotalCount(kUserClicksOnLoginHistogramName, 0);

  // Clicks on tray elements visible during lock should be recorded.
  metrics_recorder()->RecordUserTrayClick(
      LoginMetricsRecorder::TrayClickTarget::kSystemTray);
  histogram_tester_->ExpectTotalCount(kUserClicksOnLockHistogramName, 5);
  ExpectBucketCount(
      kUserClicksOnLockHistogramName,
      LoginMetricsRecorder::LockScreenUserClickTarget::kSystemTray, 1);
  histogram_tester_->ExpectTotalCount(kUserClicksOnLoginHistogramName, 0);

  metrics_recorder()->RecordUserTrayClick(
      LoginMetricsRecorder::TrayClickTarget::kVirtualKeyboardTray);
  histogram_tester_->ExpectTotalCount(kUserClicksOnLockHistogramName, 6);
  ExpectBucketCount(
      kUserClicksOnLockHistogramName,
      LoginMetricsRecorder::LockScreenUserClickTarget::kVirtualKeyboardTray, 1);
  histogram_tester_->ExpectTotalCount(kUserClicksOnLoginHistogramName, 0);

  metrics_recorder()->RecordUserTrayClick(
      LoginMetricsRecorder::TrayClickTarget::kImeTray);
  histogram_tester_->ExpectTotalCount(kUserClicksOnLockHistogramName, 7);
  ExpectBucketCount(kUserClicksOnLockHistogramName,
                    LoginMetricsRecorder::LockScreenUserClickTarget::kImeTray,
                    1);
  histogram_tester_->ExpectTotalCount(kUserClicksOnLoginHistogramName, 0);

  metrics_recorder()->RecordUserTrayClick(
      LoginMetricsRecorder::TrayClickTarget::kNotificationTray);
  histogram_tester_->ExpectTotalCount(kUserClicksOnLockHistogramName, 8);
  ExpectBucketCount(
      kUserClicksOnLockHistogramName,
      LoginMetricsRecorder::LockScreenUserClickTarget::kNotificationTray, 1);
  histogram_tester_->ExpectTotalCount(kUserClicksOnLoginHistogramName, 0);

  metrics_recorder()->RecordUserTrayClick(
      LoginMetricsRecorder::TrayClickTarget::kTrayActionNoteButton);
  histogram_tester_->ExpectTotalCount(kUserClicksOnLockHistogramName, 9);
  ExpectBucketCount(kUserClicksOnLockHistogramName,
                    LoginMetricsRecorder::LockScreenUserClickTarget::
                        kLockScreenNoteActionButton,
                    1);
  histogram_tester_->ExpectTotalCount(kUserClicksOnLoginHistogramName, 0);
}

// Verifies that user click events on the login screen is recorded correctly.
TEST_F(LoginMetricsRecorderTest, RecordUserClickEventOnLoginScreen) {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);

  // Clicks on shelf buttons visible during login should be recorded.
  histogram_tester_->ExpectTotalCount(kUserClicksOnLoginHistogramName, 0);
  metrics_recorder()->RecordUserShelfButtonClick(
      LoginMetricsRecorder::ShelfButtonClickTarget::kShutDownButton);
  histogram_tester_->ExpectTotalCount(kUserClicksOnLoginHistogramName, 1);
  ExpectBucketCount(
      kUserClicksOnLoginHistogramName,
      LoginMetricsRecorder::LoginScreenUserClickTarget::kShutDownButton, 1);
  histogram_tester_->ExpectTotalCount(kUserClicksOnLockHistogramName, 0);

  metrics_recorder()->RecordUserShelfButtonClick(
      LoginMetricsRecorder::ShelfButtonClickTarget::kRestartButton);
  histogram_tester_->ExpectTotalCount(kUserClicksOnLoginHistogramName, 2);
  ExpectBucketCount(
      kUserClicksOnLoginHistogramName,
      LoginMetricsRecorder::LoginScreenUserClickTarget::kRestartButton, 1);
  histogram_tester_->ExpectTotalCount(kUserClicksOnLockHistogramName, 0);

  metrics_recorder()->RecordUserShelfButtonClick(
      LoginMetricsRecorder::ShelfButtonClickTarget::kBrowseAsGuestButton);
  histogram_tester_->ExpectTotalCount(kUserClicksOnLoginHistogramName, 3);
  ExpectBucketCount(
      kUserClicksOnLoginHistogramName,
      LoginMetricsRecorder::LoginScreenUserClickTarget::kBrowseAsGuestButton,
      1);
  histogram_tester_->ExpectTotalCount(kUserClicksOnLockHistogramName, 0);

  metrics_recorder()->RecordUserShelfButtonClick(
      LoginMetricsRecorder::ShelfButtonClickTarget::kAddUserButton);
  histogram_tester_->ExpectTotalCount(kUserClicksOnLoginHistogramName, 4);
  ExpectBucketCount(
      kUserClicksOnLoginHistogramName,
      LoginMetricsRecorder::LoginScreenUserClickTarget::kAddUserButton, 1);
  histogram_tester_->ExpectTotalCount(kUserClicksOnLockHistogramName, 0);

  // Clicks on tray elements visible during login should be recorded.
  metrics_recorder()->RecordUserTrayClick(
      LoginMetricsRecorder::TrayClickTarget::kSystemTray);
  histogram_tester_->ExpectTotalCount(kUserClicksOnLoginHistogramName, 5);
  ExpectBucketCount(
      kUserClicksOnLoginHistogramName,
      LoginMetricsRecorder::LoginScreenUserClickTarget::kSystemTray, 1);
  histogram_tester_->ExpectTotalCount(kUserClicksOnLockHistogramName, 0);

  metrics_recorder()->RecordUserTrayClick(
      LoginMetricsRecorder::TrayClickTarget::kVirtualKeyboardTray);
  histogram_tester_->ExpectTotalCount(kUserClicksOnLoginHistogramName, 6);
  ExpectBucketCount(
      kUserClicksOnLoginHistogramName,
      LoginMetricsRecorder::LoginScreenUserClickTarget::kVirtualKeyboardTray,
      1);
  histogram_tester_->ExpectTotalCount(kUserClicksOnLockHistogramName, 0);

  metrics_recorder()->RecordUserTrayClick(
      LoginMetricsRecorder::TrayClickTarget::kImeTray);
  histogram_tester_->ExpectTotalCount(kUserClicksOnLoginHistogramName, 7);
  ExpectBucketCount(kUserClicksOnLoginHistogramName,
                    LoginMetricsRecorder::LoginScreenUserClickTarget::kImeTray,
                    1);
  histogram_tester_->ExpectTotalCount(kUserClicksOnLockHistogramName, 0);
}

}  // namespace ash
