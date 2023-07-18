// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_METRICS_LOGIN_METRICS_RECORDER_H_
#define ASH_METRICS_LOGIN_METRICS_RECORDER_H_

#include "ash/ash_export.h"

namespace ash {

// A metrics recorder that records user activity in login screen.
// This is tied to UserMetricsRecorder lifetime.
// Authentication related metrics are captured in
// chromeos/ash/components/login/auth/auth_events_recorder.h
class ASH_EXPORT LoginMetricsRecorder {
 public:
  // User clicks target on the lock screen. This enum is used to back an UMA
  // histogram and new values should be inserted immediately above kTargetCount.
  enum class LockScreenUserClickTarget {
    kShutDownButton = 0,
    kRestartButton,
    kSignOutButton,
    kCloseNoteButton,
    kSystemTray,
    kVirtualKeyboardTray,
    kImeTray,
    kNotificationTray,
    kTrayActionNoteButton,
    kParentAccessButton,
    kTargetCount,
  };

  // User clicks target on the login screen. This enum is used to back an UMA
  // histogram and new values should be inserted immediately above kTargetCount.
  enum class LoginScreenUserClickTarget {
    kShutDownButton = 0,
    kRestartButton,
    kBrowseAsGuestButton,
    kAddUserButton,
    kSystemTray,
    kVirtualKeyboardTray,
    kImeTray,
    kOsInstallButton,
    kTargetCount,
  };

  // User clicks target in OOBE. This enum is used to back an UMA
  // histogram and new values should be inserted immediately above kTargetCount.
  enum class OobeUserClickTarget {
    kShutDownButton = 0,
    kBrowseAsGuestButton,
    kSystemTray,
    kVirtualKeyboardTray,
    kImeTray,
    kEnterpriseEnrollmentButton,
    kSignIn,
    kOsInstallButton,
    kSchoolEnrollmentButton,
    kTargetCount,
  };

  // Helper enumeration for tray related user click targets on login and lock
  // screens. Values are translated to UMA histogram enums.
  enum class TrayClickTarget {
    kSystemTray,
    kVirtualKeyboardTray,
    kImeTray,
    kNotificationTray,
    kTrayActionNoteButton,
    kTargetCount,
  };

  // Helper enumeration for shelf buttons user click targets on login and lock
  // screens. Values are translated to UMA histogram enums.
  enum class ShelfButtonClickTarget {
    kShutDownButton,
    kRestartButton,
    kSignOutButton,
    kBrowseAsGuestButton,
    kAddUserButton,
    kCloseNoteButton,
    kCancelButton,
    kParentAccessButton,
    kEnterpriseEnrollmentButton,
    kOsInstallButton,
    kSignIn,
    kSchoolEnrollmentButton,
    kTargetCount,
  };

  LoginMetricsRecorder();

  LoginMetricsRecorder(const LoginMetricsRecorder&) = delete;
  LoginMetricsRecorder& operator=(const LoginMetricsRecorder&) = delete;

  ~LoginMetricsRecorder();

  // Methods used to record UMA stats.
  void RecordUserTrayClick(TrayClickTarget target);
  void RecordUserShelfButtonClick(ShelfButtonClickTarget target);
};

}  // namespace ash

#endif  // ASH_METRICS_LOGIN_METRICS_RECORDER_H_
