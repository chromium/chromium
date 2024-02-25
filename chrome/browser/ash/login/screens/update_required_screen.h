// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_UPDATE_REQUIRED_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_UPDATE_REQUIRED_SCREEN_H_

#include <memory>
#include <set>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/screens/error_screen.h"
#include "chrome/browser/ash/login/version_updater/version_updater.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"

namespace base {
class Clock;
}  // namespace base

namespace ash {

class ErrorScreensHistogramHelper;
class NetworkStateHandler;
class UpdateRequiredView;

// Controller for the update required screen.
class UpdateRequiredScreen : public BaseScreen,
                             public VersionUpdater::Delegate,
                             public NetworkStateHandlerObserver {
 public:
  using TView = UpdateRequiredView;

  UpdateRequiredScreen(base::WeakPtr<UpdateRequiredView> view,
                       ErrorScreen* error_screen,
                       base::RepeatingClosure exit_callback);

  UpdateRequiredScreen(const UpdateRequiredScreen&) = delete;
  UpdateRequiredScreen& operator=(const UpdateRequiredScreen&) = delete;

  ~UpdateRequiredScreen() override;

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

  // Exit the screen.
  void Exit();

  VersionUpdater* GetVersionUpdaterForTesting();

  // Set a base clock (used to set current time) for testing EOL.
  void SetClockForTesting(base::Clock* clock);

  void SetErrorMessageDelayForTesting(const base::TimeDelta& delay);

 private:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  void EnsureScreenIsShown();

  // Callback for changes to chromeos::kMinimumChromeVersionAueMessage.
  void OnEolMessageChanged();

  void OnSelectNetworkButtonClicked();
  void OnUpdateButtonClicked();

  // NetworkStateHandlerObserver:
  void DefaultNetworkChanged(const NetworkState* network) override;

  void RefreshNetworkState();
  void RefreshView(const VersionUpdater::UpdateInfo& update_info);

  // Subscribes to network change notifications.
  void ObserveNetworkState();

  // Unsubscribes from network change notifications.
  void StopObservingNetworkState();

  void HideErrorMessage();

  // The user requested an attempt to connect to the network should be made.
  void OnConnectRequested();

  void OnGetEolInfo(const UpdateEngineClient::EolInfo& info);

  void OnErrorScreenHidden();

  // Deletes all users data on the device.
  void DeleteUsersData();

  // True if there was no notification about captive portal state for
  // the default network.
  bool is_first_portal_notification_ = true;

  base::WeakPtr<UpdateRequiredView> view_;
  raw_ptr<ErrorScreen> error_screen_;
  base::RepeatingClosure exit_callback_;
  std::unique_ptr<ErrorScreensHistogramHelper> histogram_helper_;

  base::ScopedObservation<NetworkStateHandler, NetworkStateHandlerObserver>
      network_state_handler_observer_{this};

  // Whether the screen is shown.
  bool is_shown_ = false;

  // True if subscribed to network change notification.
  bool is_network_subscribed_ = false;

  // True if Show() has never been called yet.
  bool first_time_shown_ = true;
  bool is_updating_now_ = false;
  bool waiting_for_reboot_ = false;
  bool waiting_for_connection_ = false;
  bool metered_network_update_permission = false;

  std::unique_ptr<VersionUpdater> version_updater_;

  // Timer for the captive portal detector to show portal login page.
  // If redirect did not happen during this delay, error message is shown
  // instead.
  base::OneShotTimer error_message_timer_;

  // Overridden for testing EOL by setting the current time.
  raw_ptr<base::Clock> clock_;

  base::TimeDelta error_message_delay_;

  base::CallbackListSubscription eol_message_subscription_;

  base::CallbackListSubscription connect_request_subscription_;

  base::WeakPtrFactory<UpdateRequiredScreen> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_UPDATE_REQUIRED_SCREEN_H_
