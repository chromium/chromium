// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_UPDATE_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_UPDATE_SCREEN_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/containers/fixed_flat_set.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/screens/error_screen.h"
#include "chrome/browser/ash/login/version_updater/version_updater.h"
#include "chromeos/dbus/power/power_manager_client.h"

namespace base {
class TickClock;
}

namespace ash {

struct AccessibilityStatusEventDetails;
class ErrorScreensHistogramHelper;
class UpdateView;

// Controller for the update screen.
//
// The screen will request an update availability check from the update engine,
// and track the update engine progress. When the UpdateScreen finishes, it will
// run the `exit_callback` with the screen result.
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
class UpdateScreen : public BaseScreen,
                     public VersionUpdater::Delegate,
                     public chromeos::PowerManagerClient::Observer {
 public:
  using TView = UpdateView;
  using Result = VersionUpdater::Result;

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  UpdateScreen(base::WeakPtr<UpdateView> view,
               ErrorScreen* error_screen,
               const ScreenExitCallback& exit_callback);

  UpdateScreen(const UpdateScreen&) = delete;
  UpdateScreen& operator=(const UpdateScreen&) = delete;

  ~UpdateScreen() override;

  base::OneShotTimer* GetShowTimerForTesting();
  base::OneShotTimer* GetErrorMessageTimerForTesting();
  VersionUpdater* GetVersionUpdaterForTesting();

  void set_delay_for_delayed_timer_for_testing(base::TimeDelta delay) {
    delay_error_message_ = delay;
  }

  // VersionUpdater::Delegate:
  void OnWaitForRebootTimeElapsed() override;
  void PrepareForUpdateCheck() override;
  void ShowErrorMessage() override;
  void UpdateErrorMessage(NetworkState::PortalState state,
                          NetworkError::ErrorState error_state,
                          const std::string& network_name) override;
  void DelayErrorMessage() override;
  void UpdateInfoChanged(
      const VersionUpdater::UpdateInfo& update_info) override;
  void FinishExitUpdate(VersionUpdater::Result result) override;

  // PowerManagerClient::Observer:
  void PowerChanged(const power_manager::PowerSupplyProperties& proto) override;

  void set_exit_callback_for_testing(ScreenExitCallback exit_callback) {
    exit_callback_ = exit_callback;
  }

  void set_tick_clock_for_testing(const base::TickClock* tick_clock) {
    tick_clock_ = tick_clock;
  }

  void set_wait_before_reboot_time_for_testing(
      base::TimeDelta wait_before_reboot_time) {
    wait_before_reboot_time_ = wait_before_reboot_time;
  }

  void set_show_delay_for_testing(base::TimeDelta show_delay) {
    show_delay_ = show_delay;
  }

  base::OneShotTimer* GetWaitRebootTimerForTesting() {
    return &wait_reboot_timer_;
  }

 protected:
  // BaseScreen:
  bool MaybeSkip(WizardContext& context) override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  void ExitUpdate(Result result);

 private:
  FRIEND_TEST_ALL_PREFIXES(UpdateScreenTest, TestBasic);
  FRIEND_TEST_ALL_PREFIXES(UpdateScreenTest, TestUpdateAvailable);
  FRIEND_TEST_ALL_PREFIXES(UpdateScreenTest, TestAPReselection);
  friend class UpdateScreenUnitTest;

  // Returns true if there is critical system update that requires installation
  // and immediate reboot.
  bool HasCriticalUpdate();

  // Checks that screen is shown, shows if not.
  void MakeSureScreenIsShown();

  void HideErrorMessage();

  // The user requested an attempt to connect to the network should be made.
  void OnConnectRequested();

  // Notification of a change in the accessibility settings.
  void OnAccessibilityStatusChanged(
      const AccessibilityStatusEventDetails& details);

  // Callback passed to `error_screen_` when it's shown. Called when the error
  // screen gets hidden.
  void OnErrorScreenHidden();

  // Updates visibility of the low battery warning message during the update
  // stages. Called when power or update status changes.
  void UpdateBatteryWarningVisibility();

  // Show reboot waiting screen.
  void ShowRebootInProgress();

  // Set update status message.
  void SetUpdateStatusMessage(int percent, base::TimeDelta time_left);

  // Determines if the device is in EU zone to show info about opt out.
  static bool CheckIfOptOutIsEnabled();

  base::WeakPtr<UpdateView> view_;
  raw_ptr<ErrorScreen> error_screen_;
  ScreenExitCallback exit_callback_;

  // Whether the update screen is shown.
  bool is_shown_ = false;

  // True if there was no notification about captive portal state for
  // the default network.
  bool is_first_portal_notification_ = true;

  // True if already checked that update is critical.
  bool is_critical_checked_ = false;

  // Caches the result of HasCriticalUpdate function.
  std::optional<bool> has_critical_update_;

  // True if the update progress should be hidden even if update_info suggests
  // the opposite.
  bool hide_progress_on_exit_ = false;
  // True if it is possible for user to skip update check.
  bool cancel_update_shortcut_enabled_ = false;

  // Determines if we should show additional info during update or right after
  // check for update is done.
  bool is_opt_out_enabled_ = false;

  // Whether Quick Start was notified of an update. True for users who
  // previously started Quick Start and will install an update.
  bool did_prepare_quick_start_for_update_ = false;

  // EU country list.
  inline static constexpr auto kEUCountriesSet =
      base::MakeFixedFlatSet<std::string_view>(
          {"at", "be", "bg", "hr", "cy", "cz", "dk", "ee", "fi",
           "fr", "de", "gr", "hu", "ie", "it", "lv", "lt", "lu",
           "mt", "nl", "pl", "pt", "ro", "sk", "si", "es", "se"});

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

  // Timer for the interval to wait for the reboot progress screen to be shown
  // for at least wait_before_reboot_time_ before reboot call.
  base::OneShotTimer wait_reboot_timer_;

  // Time in seconds after which we initiate reboot.
  base::TimeDelta wait_before_reboot_time_;

  // Time to delay showing the screen.
  base::TimeDelta show_delay_;

  raw_ptr<const base::TickClock> tick_clock_;

  base::TimeTicks start_update_downloading_;
  // Support variables for update stages time recording.
  base::TimeTicks start_update_stage_;
  base::TimeDelta check_time_;
  base::TimeDelta download_time_;
  base::TimeDelta verify_time_;
  base::TimeDelta finalize_time_;

  base::CallbackListSubscription connect_request_subscription_;

  base::CallbackListSubscription accessibility_subscription_;

  // Delay before showing error message if captive portal is detected.
  // We wait for this delay to let captive portal to perform redirect and show
  // its login page before error message appears.
  base::TimeDelta delay_error_message_ = base::Seconds(10);

  // PowerManagerClient::Observer is used only when screen is shown.
  base::ScopedObservation<chromeos::PowerManagerClient,
                          chromeos::PowerManagerClient::Observer>
      power_manager_subscription_{this};

  base::WeakPtrFactory<UpdateScreen> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_UPDATE_SCREEN_H_
