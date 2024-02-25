// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_SESSION_LOGOUT_CONFIRMATION_CONTROLLER_H_
#define ASH_SYSTEM_SESSION_LOGOUT_CONFIRMATION_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

class PrefRegistrySimple;

namespace base {
class TickClock;
}

namespace ash {

class LogoutConfirmationDialog;

// This class shows a dialog asking the user to confirm or deny logout and
// terminates the session if the user either confirms or allows the countdown
// shown in the dialog to expire.
//
// It is guaranteed that no more than one confirmation dialog will be visible at
// any given time. If there are multiple requests to show a confirmation dialog
// at the same time, the dialog whose countdown expires first is shown.
//
// In public sessions, asks the user to end the session when the last window is
// closed.
class ASH_EXPORT LogoutConfirmationController : public SessionObserver {
 public:
  enum class Source { kShelfExitButton, kCloseAllWindows };

  LogoutConfirmationController();

  LogoutConfirmationController(const LogoutConfirmationController&) = delete;
  LogoutConfirmationController& operator=(const LogoutConfirmationController&) =
      delete;

  ~LogoutConfirmationController() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  const base::TickClock* clock() const { return clock_; }

  // Shows a LogoutConfirmationDialog. If a confirmation dialog is already being
  // shown, it is closed and a new one opened if |logout_time| is earlier than
  // the current dialog's |logout_time_|.
  void ConfirmLogout(base::TimeTicks logout_time, Source source);

  // SessionObserver:
  void OnLoginStatusChanged(LoginStatus login_status) override;
  void OnLockStateChanged(bool locked) override;

  // Called by the |dialog_| when the user confirms logout.
  void OnLogoutConfirmed();

  // Called by the |dialog_| when it is closed.
  void OnDialogClosed();

  // Overrides the internal clock for testing. This doesn't take the ownership
  // of the clock. |clock| must outlive the LogoutConfirmationController
  // instance.
  void SetClockForTesting(const base::TickClock* clock);
  void SetLogoutCallbackForTesting(
      const base::RepeatingCallback<void(Source)>& logout_callback);
  LogoutConfirmationDialog* dialog_for_testing() const { return dialog_; }

  int confirm_logout_count_for_test() const {
    return confirm_logout_count_for_test_;
  }

 private:
  class LastWindowClosedObserver;
  std::unique_ptr<LastWindowClosedObserver> last_window_closed_observer_;

  raw_ptr<const base::TickClock> clock_;

  base::RepeatingCallback<void(Source)> logout_callback_;
  Source source_;

  base::TimeTicks logout_time_;
  raw_ptr<LogoutConfirmationDialog> dialog_ =
      nullptr;  // Owned by the Views hierarchy.
  base::OneShotTimer logout_timer_;

  int confirm_logout_count_for_test_ = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_SESSION_LOGOUT_CONFIRMATION_CONTROLLER_H_
