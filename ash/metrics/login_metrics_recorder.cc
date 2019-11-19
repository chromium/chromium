// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/login_metrics_recorder.h"

#include "ash/login/ui/lock_screen.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"

namespace ash {

namespace {

void LogUserClickOnLock(
    LoginMetricsRecorder::LockScreenUserClickTarget target) {
  DCHECK_NE(LoginMetricsRecorder::LockScreenUserClickTarget::kTargetCount,
            target);
  UMA_HISTOGRAM_ENUMERATION(
      "Ash.Login.Lock.UserClicks", target,
      LoginMetricsRecorder::LockScreenUserClickTarget::kTargetCount);
}

void LogUserClickOnLogin(
    LoginMetricsRecorder::LoginScreenUserClickTarget target) {
  DCHECK_NE(LoginMetricsRecorder::LoginScreenUserClickTarget::kTargetCount,
            target);
  UMA_HISTOGRAM_ENUMERATION(
      "Ash.Login.Login.UserClicks", target,
      LoginMetricsRecorder::LoginScreenUserClickTarget::kTargetCount);
}

void LogUserClick(
    LoginMetricsRecorder::LockScreenUserClickTarget lock_target,
    LoginMetricsRecorder::LoginScreenUserClickTarget login_target) {
  bool is_locked = Shell::Get()->session_controller()->GetSessionState() ==
                   session_manager::SessionState::LOCKED;
  if (is_locked) {
    LogUserClickOnLock(lock_target);
  } else {
    LogUserClickOnLogin(login_target);
  }
}

bool ShouldRecordMetrics() {
  session_manager::SessionState session_state =
      Shell::Get()->session_controller()->GetSessionState();
  return session_state == session_manager::SessionState::LOGIN_PRIMARY ||
         session_state == session_manager::SessionState::LOCKED;
}

}  // namespace

LoginMetricsRecorder::LoginMetricsRecorder() = default;
LoginMetricsRecorder::~LoginMetricsRecorder() = default;

void LoginMetricsRecorder::RecordNumLoginAttempts(int num_attempt,
                                                  bool success) {
  if (success) {
    UMA_HISTOGRAM_COUNTS_100("Ash.Login.Lock.NumPasswordAttempts.UntilSuccess",
                             num_attempt);
  } else {
    UMA_HISTOGRAM_COUNTS_100("Ash.Login.Lock.NumPasswordAttempts.UntilFailure",
                             num_attempt);
  }
}

void LoginMetricsRecorder::RecordUserTrayClick(TrayClickTarget target) {
  if (!ShouldRecordMetrics())
    return;

  bool is_locked = Shell::Get()->session_controller()->GetSessionState() ==
                   session_manager::SessionState::LOCKED;
  switch (target) {
    case TrayClickTarget::kSystemTray:
      LogUserClick(LockScreenUserClickTarget::kSystemTray,
                   LoginScreenUserClickTarget::kSystemTray);
      break;
    case TrayClickTarget::kVirtualKeyboardTray:
      LogUserClick(LockScreenUserClickTarget::kVirtualKeyboardTray,
                   LoginScreenUserClickTarget::kVirtualKeyboardTray);
      break;
    case TrayClickTarget::kImeTray:
      LogUserClick(LockScreenUserClickTarget::kImeTray,
                   LoginScreenUserClickTarget::kImeTray);
      break;
    case TrayClickTarget::kNotificationTray:
      DCHECK(is_locked);
      LogUserClick(LockScreenUserClickTarget::kNotificationTray,
                   LoginScreenUserClickTarget::kTargetCount);
      break;
    case TrayClickTarget::kTrayActionNoteButton:
      DCHECK(is_locked);
      LogUserClick(LockScreenUserClickTarget::kLockScreenNoteActionButton,
                   LoginScreenUserClickTarget::kTargetCount);
      break;
    case TrayClickTarget::kTargetCount:
      NOTREACHED();
      break;
  }
}

void LoginMetricsRecorder::RecordUserShelfButtonClick(
    ShelfButtonClickTarget target) {
  if (!ShouldRecordMetrics())
    return;

  bool is_lock = Shell::Get()->session_controller()->GetSessionState() ==
                 session_manager::SessionState::LOCKED;
  switch (target) {
    case ShelfButtonClickTarget::kShutDownButton:
      LogUserClick(LockScreenUserClickTarget::kShutDownButton,
                   LoginScreenUserClickTarget::kShutDownButton);
      break;
    case ShelfButtonClickTarget::kRestartButton:
      LogUserClick(LockScreenUserClickTarget::kRestartButton,
                   LoginScreenUserClickTarget::kRestartButton);
      break;
    case ShelfButtonClickTarget::kSignOutButton:
      DCHECK(is_lock);
      LogUserClickOnLock(LockScreenUserClickTarget::kSignOutButton);
      break;
    case ShelfButtonClickTarget::kBrowseAsGuestButton:
      DCHECK(!is_lock);
      LogUserClickOnLogin(LoginScreenUserClickTarget::kBrowseAsGuestButton);
      break;
    case ShelfButtonClickTarget::kAddUserButton:
      DCHECK(!is_lock);
      LogUserClickOnLogin(LoginScreenUserClickTarget::kAddUserButton);
      break;
    case ShelfButtonClickTarget::kCloseNoteButton:
      DCHECK(is_lock);
      LogUserClickOnLock(LockScreenUserClickTarget::kCloseNoteButton);
      break;
    case ShelfButtonClickTarget::kCancelButton:
      // Should not be called in LOCKED nor LOGIN_PRIMARY states.
      NOTREACHED();
      break;
    case ShelfButtonClickTarget::kParentAccessButton:
      DCHECK(is_lock);
      LogUserClickOnLock(LockScreenUserClickTarget::kParentAccessButton);
      break;
    case ShelfButtonClickTarget::kTargetCount:
      NOTREACHED();
      break;
  }
}

}  // namespace ash
