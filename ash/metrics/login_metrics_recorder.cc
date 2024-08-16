// Copyright 2017 The Chromium Authors
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
  UMA_HISTOGRAM_ENUMERATION(
      "Ash.Login.Lock.UserClicks", target,
      LoginMetricsRecorder::LockScreenUserClickTarget::kTargetCount);
}

void LogUserClickOnLogin(
    LoginMetricsRecorder::LoginScreenUserClickTarget target) {
  UMA_HISTOGRAM_ENUMERATION(
      "Ash.Login.Login.UserClicks", target,
      LoginMetricsRecorder::LoginScreenUserClickTarget::kTargetCount);
}

void LogUserClickInOobe(LoginMetricsRecorder::OobeUserClickTarget target) {
  UMA_HISTOGRAM_ENUMERATION(
      "Ash.Login.OOBE.UserClicks", target,
      LoginMetricsRecorder::OobeUserClickTarget::kTargetCount);
}

// Defines mapping of ShelfButtonClickTarget |original| to different UMA target
// in different session state.
struct ShelfButtonClickMapping {
  LoginMetricsRecorder::ShelfButtonClickTarget original;
  LoginMetricsRecorder::LockScreenUserClickTarget lock;
  LoginMetricsRecorder::LoginScreenUserClickTarget login;
  LoginMetricsRecorder::OobeUserClickTarget oobe;
};

// |kTargetCount| is used to mark the click target as unexpected.
const ShelfButtonClickMapping kShelfTargets[] = {
    // |kShutDownButton|
    {LoginMetricsRecorder::ShelfButtonClickTarget::kShutDownButton,
     LoginMetricsRecorder::LockScreenUserClickTarget::kShutDownButton,
     LoginMetricsRecorder::LoginScreenUserClickTarget::kShutDownButton,
     LoginMetricsRecorder::OobeUserClickTarget::kShutDownButton},
    // |kRestartButton|
    {LoginMetricsRecorder::ShelfButtonClickTarget::kRestartButton,
     LoginMetricsRecorder::LockScreenUserClickTarget::kRestartButton,
     LoginMetricsRecorder::LoginScreenUserClickTarget::kRestartButton,
     LoginMetricsRecorder::OobeUserClickTarget::kTargetCount},
    // |kSignOutButton|
    {LoginMetricsRecorder::ShelfButtonClickTarget::kSignOutButton,
     LoginMetricsRecorder::LockScreenUserClickTarget::kSignOutButton,
     LoginMetricsRecorder::LoginScreenUserClickTarget::kTargetCount,
     LoginMetricsRecorder::OobeUserClickTarget::kTargetCount},
    // |kBrowseAsGuestButton|
    {LoginMetricsRecorder::ShelfButtonClickTarget::kBrowseAsGuestButton,
     LoginMetricsRecorder::LockScreenUserClickTarget::kTargetCount,
     LoginMetricsRecorder::LoginScreenUserClickTarget::kBrowseAsGuestButton,
     LoginMetricsRecorder::OobeUserClickTarget::kBrowseAsGuestButton},
    // |kAddUserButton|
    {LoginMetricsRecorder::ShelfButtonClickTarget::kAddUserButton,
     LoginMetricsRecorder::LockScreenUserClickTarget::kTargetCount,
     LoginMetricsRecorder::LoginScreenUserClickTarget::kAddUserButton,
     LoginMetricsRecorder::OobeUserClickTarget::kTargetCount},
    // |kCloseNoteButton|
    {LoginMetricsRecorder::ShelfButtonClickTarget::kCloseNoteButton,
     LoginMetricsRecorder::LockScreenUserClickTarget::kCloseNoteButton,
     LoginMetricsRecorder::LoginScreenUserClickTarget::kTargetCount,
     LoginMetricsRecorder::OobeUserClickTarget::kTargetCount},
    // |kParentAccessButton|
    {LoginMetricsRecorder::ShelfButtonClickTarget::kParentAccessButton,
     LoginMetricsRecorder::LockScreenUserClickTarget::kParentAccessButton,
     LoginMetricsRecorder::LoginScreenUserClickTarget::kTargetCount,
     LoginMetricsRecorder::OobeUserClickTarget::kTargetCount},
    // |kEnterpriseEnrollmentButton|
    {LoginMetricsRecorder::ShelfButtonClickTarget::kEnterpriseEnrollmentButton,
     LoginMetricsRecorder::LockScreenUserClickTarget::kTargetCount,
     LoginMetricsRecorder::LoginScreenUserClickTarget::kTargetCount,
     LoginMetricsRecorder::OobeUserClickTarget::kEnterpriseEnrollmentButton},
    // |kOsInstallButton|
    {LoginMetricsRecorder::ShelfButtonClickTarget::kOsInstallButton,
     LoginMetricsRecorder::LockScreenUserClickTarget::kTargetCount,
     LoginMetricsRecorder::LoginScreenUserClickTarget::kOsInstallButton,
     LoginMetricsRecorder::OobeUserClickTarget::kOsInstallButton},
    // |kSignIn|
    {LoginMetricsRecorder::ShelfButtonClickTarget::kSignIn,
     LoginMetricsRecorder::LockScreenUserClickTarget::kTargetCount,
     LoginMetricsRecorder::LoginScreenUserClickTarget::kTargetCount,
     LoginMetricsRecorder::OobeUserClickTarget::kSignIn},
    // |kSchoolEnrollmentButton|
    {LoginMetricsRecorder::ShelfButtonClickTarget::kSchoolEnrollmentButton,
     LoginMetricsRecorder::LockScreenUserClickTarget::kTargetCount,
     LoginMetricsRecorder::LoginScreenUserClickTarget::kTargetCount,
     LoginMetricsRecorder::OobeUserClickTarget::kSchoolEnrollmentButton},
};

// Defines mapping of TrayClickTarget |original| to different UMA target in
// different session state.
struct TrayClickMapping {
  LoginMetricsRecorder::TrayClickTarget original;
  LoginMetricsRecorder::LockScreenUserClickTarget lock;
  LoginMetricsRecorder::LoginScreenUserClickTarget login;
  LoginMetricsRecorder::OobeUserClickTarget oobe;
};

// |kTargetCount| is used to mark the click target as unexpected.
const TrayClickMapping kTrayTargets[] = {
    // |kSystemTray|
    {LoginMetricsRecorder::TrayClickTarget::kSystemTray,
     LoginMetricsRecorder::LockScreenUserClickTarget::kSystemTray,
     LoginMetricsRecorder::LoginScreenUserClickTarget::kSystemTray,
     LoginMetricsRecorder::OobeUserClickTarget::kSystemTray},
    // |kVirtualKeyboardTray|
    {LoginMetricsRecorder::TrayClickTarget::kVirtualKeyboardTray,
     LoginMetricsRecorder::LockScreenUserClickTarget::kVirtualKeyboardTray,
     LoginMetricsRecorder::LoginScreenUserClickTarget::kVirtualKeyboardTray,
     LoginMetricsRecorder::OobeUserClickTarget::kVirtualKeyboardTray},
    // |kImeTray|
    {LoginMetricsRecorder::TrayClickTarget::kImeTray,
     LoginMetricsRecorder::LockScreenUserClickTarget::kImeTray,
     LoginMetricsRecorder::LoginScreenUserClickTarget::kImeTray,
     LoginMetricsRecorder::OobeUserClickTarget::kImeTray},
    // |kNotificationTray|
    {LoginMetricsRecorder::TrayClickTarget::kNotificationTray,
     LoginMetricsRecorder::LockScreenUserClickTarget::kNotificationTray,
     LoginMetricsRecorder::LoginScreenUserClickTarget::kTargetCount,
     LoginMetricsRecorder::OobeUserClickTarget::kTargetCount},
    // |kTrayActionNoteButton|
    {LoginMetricsRecorder::TrayClickTarget::kTrayActionNoteButton,
     LoginMetricsRecorder::LockScreenUserClickTarget::kTrayActionNoteButton,
     LoginMetricsRecorder::LoginScreenUserClickTarget::kTargetCount,
     LoginMetricsRecorder::OobeUserClickTarget::kTargetCount},
};

bool ShouldRecordMetrics() {
  session_manager::SessionState session_state =
      Shell::Get()->session_controller()->GetSessionState();
  return session_state == session_manager::SessionState::LOGIN_PRIMARY ||
         session_state == session_manager::SessionState::LOCKED ||
         session_state == session_manager::SessionState::OOBE;
}

}  // namespace

LoginMetricsRecorder::LoginMetricsRecorder() = default;
LoginMetricsRecorder::~LoginMetricsRecorder() = default;

void LoginMetricsRecorder::RecordUserTrayClick(TrayClickTarget target) {
  if (!ShouldRecordMetrics())
    return;
  auto state = Shell::Get()->session_controller()->GetSessionState();
  for (const auto& el : kTrayTargets) {
    if (el.original != target)
      continue;
    switch (state) {
      case session_manager::SessionState::LOCKED:
        DCHECK(el.lock != LockScreenUserClickTarget::kTargetCount)
            << "Not expected tray click target: " << static_cast<int>(target)
            << " for session state: " << static_cast<int>(state);
        LogUserClickOnLock(el.lock);
        return;
      case session_manager::SessionState::LOGIN_PRIMARY:
        DCHECK(el.login != LoginScreenUserClickTarget::kTargetCount)
            << "Not expected tray click target: " << static_cast<int>(target)
            << " for session state: " << static_cast<int>(state);
        LogUserClickOnLogin(el.login);
        return;
      case session_manager::SessionState::OOBE:
        DCHECK(el.oobe != OobeUserClickTarget::kTargetCount)
            << "Not expected tray click target: " << static_cast<int>(target)
            << " for session state: " << static_cast<int>(state);
        LogUserClickInOobe(el.oobe);
        return;
      default:
        NOTREACHED() << "Unexpected session state: " << static_cast<int>(state);
    }
  }
  NOTREACHED() << "Tray click target wasn't found in the |kTrayTargets|.";
}

void LoginMetricsRecorder::RecordUserShelfButtonClick(
    ShelfButtonClickTarget target) {
  if (!ShouldRecordMetrics())
    return;
  auto state = Shell::Get()->session_controller()->GetSessionState();
  for (const auto& el : kShelfTargets) {
    if (el.original != target)
      continue;
    switch (state) {
      case session_manager::SessionState::LOCKED:
        DCHECK(el.lock != LockScreenUserClickTarget::kTargetCount)
            << "Not expected shelf click target: " << static_cast<int>(target)
            << " for session state: " << static_cast<int>(state);
        LogUserClickOnLock(el.lock);
        return;
      case session_manager::SessionState::LOGIN_PRIMARY:
        DCHECK(el.login != LoginScreenUserClickTarget::kTargetCount)
            << "Not expected shelf click target: " << static_cast<int>(target)
            << " for session state: " << static_cast<int>(state);
        LogUserClickOnLogin(el.login);
        return;
      case session_manager::SessionState::OOBE:
        DCHECK(el.oobe != OobeUserClickTarget::kTargetCount)
            << "Not expected shelf click target: " << static_cast<int>(target)
            << " for session state: " << static_cast<int>(state);
        LogUserClickInOobe(el.oobe);
        return;
      default:
        NOTREACHED() << "Unexpected session state: " << static_cast<int>(state);
    }
  }
  NOTREACHED() << "Shelf click target wasn't found in the |kShelfTargets|.";
}

}  // namespace ash
