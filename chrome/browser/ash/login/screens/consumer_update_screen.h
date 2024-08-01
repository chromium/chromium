// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_CONSUMER_UPDATE_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_CONSUMER_UPDATE_SCREEN_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/screens/error_screen.h"
#include "chrome/browser/ash/login/screens/oobe_mojo_binder.h"
#include "chrome/browser/ash/login/version_updater/version_updater.h"
#include "chrome/browser/ui/webui/ash/login/mojom/screens_oobe.mojom.h"
#include "chromeos/dbus/power/power_manager_client.h"

namespace ash {

class ConsumerUpdateScreenView;
class ErrorScreensHistogramHelper;
struct AccessibilityStatusEventDetails;

// Controller for the Consumer update screen.
class ConsumerUpdateScreen
    : public BaseScreen,
      public VersionUpdater::Delegate,
      public chromeos::PowerManagerClient::Observer,
      public screens_oobe::mojom::ConsumerUpdatePageHandler,
      public OobeMojoBinder<screens_oobe::mojom::ConsumerUpdatePageHandler,
                            screens_oobe::mojom::ConsumerUpdatePage> {
 public:
  using TView = ConsumerUpdateScreenView;

  enum class Result {
    BACK,
    UPDATED,
    SKIPPED,
    DECLINE_CELLULAR,
    UPDATE_NOT_REQUIRED,
    UPDATE_ERROR,
    NOT_APPLICABLE,
    CHECK_TIMEOUT,
  };

  // This enum is tied directly to the OobeConsumerUpdateScreenSkippedReason UMA
  // enum defined in //tools/metrics/histograms/enums.xml, and should always
  // reflect it (do not change one without changing the other).  Entries should
  // be never modified or deleted.  Only additions possible.
  enum class OobeConsumerUpdateScreenSkippedReason {
    kCriticalUpdateCompleted = 0,
    kUpdateNotRequired = 1,
    kUpdateError = 2,
    kDeclineCellular = 3,
    kMaxValue = kDeclineCellular,
  };

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  ConsumerUpdateScreen(base::WeakPtr<ConsumerUpdateScreenView> view,
                       ErrorScreen* error_screen,
                       const ScreenExitCallback& exit_callback);

  ConsumerUpdateScreen(const ConsumerUpdateScreen&) = delete;
  ConsumerUpdateScreen& operator=(const ConsumerUpdateScreen&) = delete;

  ~ConsumerUpdateScreen() override;

  static std::string GetResultString(Result result);

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

  VersionUpdater* get_version_updater_for_testing() {
    return version_updater_.get();
  }

  void set_delay_for_delayed_timer_for_testing(base::TimeDelta delay) {
    delay_error_message_ = delay;
  }

  void set_delay_for_exit_no_update_for_testing(base::TimeDelta delay) {
    exit_delay_ = delay;
  }

  void set_delay_for_show_skip_button_for_testing(base::TimeDelta delay) {
    delay_skip_button_time_ = delay;
  }

  void set_maximum_time_force_update_for_testing(base::TimeDelta delay) {
    maximum_time_force_update_ = delay;
  }

  const ScreenExitCallback& get_exit_callback_for_testing() {
    return exit_callback_;
  }

  void set_exit_callback_for_testing(ScreenExitCallback exit_callback) {
    exit_callback_ = exit_callback;
  }

  void set_wait_before_reboot_time_for_testing(
      base::TimeDelta wait_before_reboot_time) {
    wait_before_reboot_time_ = wait_before_reboot_time;
  }

  base::OneShotTimer* get_error_message_timer_for_testing() {
    return &error_message_timer_;
  }

  base::OneShotTimer* get_wait_reboot_timer_for_testing() {
    return &wait_reboot_timer_;
  }

 private:
  // BaseScreen:
  bool MaybeSkip(WizardContext& context) override;
  void ShowImpl() override;
  void HideImpl() override;

  void ExitUpdate(VersionUpdater::Result result);

  // screens_oobe::mojom::ConsumerUpdatePageHandler:
  void OnDeclineCellularClicked() override;
  void OnAcceptCellularClicked() override;
  void OnSkipClicked() override;
  void OnBackClicked() override;

  void HideErrorMessage();

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

  void DelayExitNoUpdate();
  void DelaySkipButton();
  void SetSkipButton();

  void RecordOobeConsumerUpdateScreenSkippedReasonHistogram(
      OobeConsumerUpdateScreenSkippedReason reason);

  bool update_available = false;

  std::optional<bool> is_mandatory_update_;

  // True if there was no notification about captive portal state for
  // the default network.
  bool is_first_portal_notification_ = true;

  // Whether Quick Start was notified of an update. True for users who
  // previously started Quick Start and will install an update.
  bool did_prepare_quick_start_for_update_ = false;

  base::WeakPtr<ConsumerUpdateScreenView> view_;
  raw_ptr<ErrorScreen> error_screen_;
  ScreenExitCallback exit_callback_;

  base::CallbackListSubscription accessibility_subscription_;

  std::unique_ptr<ErrorScreensHistogramHelper> histogram_helper_;
  std::unique_ptr<VersionUpdater> version_updater_;

  // Timer for the captive portal detector to show portal login page.
  // If redirect did not happen during this delay, error message is shown
  // instead.
  base::OneShotTimer error_message_timer_;

  // Delay before showing error message if captive portal is detected.
  // We wait for this delay to let captive portal to perform redirect and show
  // its login page before error message appears.
  base::TimeDelta delay_error_message_ = base::Seconds(10);

  // Timer for the interval to wait for the reboot progress screen to be shown
  // for at least wait_before_reboot_time_ before reboot call.
  base::OneShotTimer wait_reboot_timer_;

  // Time in seconds after which we initiate reboot.
  base::TimeDelta wait_before_reboot_time_;

  // Timer for the consumer update screen to show skip button .
  // If applicable
  base::OneShotTimer delay_skip_button_timer_;

  // Delay to show the Skip button
  base::TimeDelta delay_skip_button_time_ = base::Seconds(15);

  // Maximum time estimate to force update
  base::TimeDelta maximum_time_force_update_ = base::Minutes(8);

  base::TimeTicks screen_shown_time_;

  // Time to delay exiting the screen to avoid flashing the screen when no
  // update is available.
  base::TimeDelta exit_delay_ = base::Seconds(2);

  // Timer for the interval to wait to exit screen when no update.
  base::OneShotTimer wait_exit_timer_;

  // Time estimated to finish the updating.
  base::TimeDelta estimate_update_time_left_;

  // PowerManagerClient::Observer is used only when screen is shown.
  base::ScopedObservation<chromeos::PowerManagerClient,
                          chromeos::PowerManagerClient::Observer>
      power_manager_subscription_{this};

  base::WeakPtrFactory<ConsumerUpdateScreen> weak_factory_{this};
};

}  // namespace ash

// TODO(crbug.com/40163357): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash ::ConsumerUpdateScreen;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_CONSUMER_UPDATE_SCREEN_H_
