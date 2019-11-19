// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_UPDATE_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_UPDATE_SCREEN_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "chrome/browser/chromeos/login/screens/error_screen.h"
#include "chrome/browser/chromeos/login/version_updater/version_updater.h"

namespace base {
class TickClock;
}

namespace chromeos {

class ErrorScreensHistogramHelper;
class ScreenManager;
class UpdateView;

// Controller for the update screen.
//
// The screen will request an update availability check from the update engine,
// and track the update engine progress. When the UpdateScreen finishes, it will
// run the |exit_callback| with the screen result.
//
// If the update engine reports no updates are found, or the available
// update is not critical, UpdateScreen will report UPDATE_NOT_REQUIRED result.
//
// If the update engine reports an error while performing a critical update,
// UpdateScreen will report UPDATE_ERROR result.
//
// If the update engine reports that update is blocked because it would be
// performed over a metered network, UpdateScreen will request user consent
// before proceeding with the update. If the user rejects, UpdateScreen will
// exit and report UPDATE_ERROR result.
//
// If update engine finds a critical update, UpdateScreen will wait for the
// update engine to apply the update, and then request a reboot (if reboot
// request times out, a message requesting manual reboot will be shown to the
// user).
//
// Before update check request is made, the screen will ensure that the device
// has network connectivity - if the current network is not online (e.g. behind
// a protal), it will request an ErrorScreen to be shown. Update check will be
// delayed until the Internet connectivity is established.
class UpdateScreen : public BaseScreen, public VersionUpdater::Delegate {
 public:
  using Result = VersionUpdater::Result;

  static UpdateScreen* Get(ScreenManager* manager);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;
  UpdateScreen(UpdateView* view,
               ErrorScreen* error_screen,
               const ScreenExitCallback& exit_callback);
  ~UpdateScreen() override;

  // Called when the being destroyed. This should call Unbind() on the
  // associated View if this class is destroyed before it.
  void OnViewDestroyed(UpdateView* view);

  // BaseScreen:
  void Show() override;
  void Hide() override;
  void OnUserAction(const std::string& action_id) override;

  base::OneShotTimer* GetShowTimerForTesting();
  base::OneShotTimer* GetErrorMessageTimerForTesting();
  VersionUpdater* GetVersionUpdaterForTesting();

  void set_ignore_update_deadlines_for_testing(bool ignore_update_deadlines) {
    ignore_update_deadlines_ = ignore_update_deadlines;
  }

  // VersionUpdater::Delegate:
  void OnWaitForRebootTimeElapsed() override;
  void PrepareForUpdateCheck() override;
  void ShowErrorMessage() override;
  void UpdateErrorMessage(
      const NetworkPortalDetector::CaptivePortalStatus status,
      const NetworkError::ErrorState& error_state,
      const std::string& network_name) override;
  void DelayErrorMessage() override;
  void UpdateInfoChanged(
      const VersionUpdater::UpdateInfo& update_info) override;
  void FinishExitUpdate(VersionUpdater::Result result) override;

 protected:
  void ExitUpdate(Result result);

 private:
  FRIEND_TEST_ALL_PREFIXES(UpdateScreenTest, TestBasic);
  FRIEND_TEST_ALL_PREFIXES(UpdateScreenTest, TestUpdateAvailable);
  FRIEND_TEST_ALL_PREFIXES(UpdateScreenTest, TestAPReselection);
  friend class UpdateScreenUnitTest;

  void RefreshView(const VersionUpdater::UpdateInfo& update_info);

  // Returns true if there is critical system update that requires installation
  // and immediate reboot.
  bool HasCriticalUpdate();

  // Checks that screen is shown, shows if not.
  void MakeSureScreenIsShown();

  void HideErrorMessage();

  // The user requested an attempt to connect to the network should be made.
  void OnConnectRequested();

  // Callback passed to |error_screen_| when it's shown. Called when the error
  // screen gets hidden.
  void OnErrorScreenHidden();

  UpdateView* view_;
  ErrorScreen* error_screen_;
  ScreenExitCallback exit_callback_;

  // If true, update deadlines are ignored.
  // Note, this is false by default.
  bool ignore_update_deadlines_ = false;
  // Whether the update screen is shown.
  bool is_shown_ = false;

  // True if there was no notification about captive portal state for
  // the default network.
  bool is_first_portal_notification_ = true;

  // True if already checked that update is critical.
  bool is_critical_checked_ = false;

  // True if the update progress should be hidden even if update_info suggests
  // the opposite.
  bool hide_progress_on_exit_ = false;
  // True if it is possible for user to skip update check.
  bool cancel_update_shortcut_enabled_ = false;

  std::unique_ptr<ErrorScreensHistogramHelper> histogram_helper_;

  std::unique_ptr<VersionUpdater> version_updater_;

  // Showing the update screen view will be delayed for a small amount of time
  // after UpdateScreen::Show() is called. If the screen determines that an
  // update is not required before the delay expires, the UpdateScreen will exit
  // without actually showing any UI. The goal is to avoid short flashes of
  // update screen UI when update check is done quickly enough.
  // This holds the timer to show the actual update screen UI.
  base::OneShotTimer show_timer_;

  // Timer for the captive portal detector to show portal login page.
  // If redirect did not happen during this delay, error message is shown
  // instead.
  base::OneShotTimer error_message_timer_;

  ErrorScreen::ConnectRequestCallbackSubscription connect_request_subscription_;

  base::WeakPtrFactory<UpdateScreen> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(UpdateScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_UPDATE_SCREEN_H_
