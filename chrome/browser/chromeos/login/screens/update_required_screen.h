// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_UPDATE_REQUIRED_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_UPDATE_REQUIRED_SCREEN_H_

#include <memory>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "chrome/browser/chromeos/login/screens/error_screen.h"
#include "chrome/browser/chromeos/login/version_updater/version_updater.h"

namespace base {
class Clock;
}  // namespace base

namespace chromeos {

class ErrorScreensHistogramHelper;
class UpdateRequiredView;

namespace login {
class NetworkStateHelper;
}  // namespace login

// Controller for the update required screen.
class UpdateRequiredScreen : public BaseScreen,
                             public VersionUpdater::Delegate,
                             public NetworkStateHandlerObserver {
 public:
  explicit UpdateRequiredScreen(UpdateRequiredView* view,
                                ErrorScreen* error_screen);
  ~UpdateRequiredScreen() override;

  // Called when the being destroyed. This should call Unbind() on the
  // associated View if this class is destroyed before it.
  void OnViewDestroyed(UpdateRequiredView* view);

  // BaseScreen:
  void Show() override;
  void Hide() override;
  void OnUserAction(const std::string& action_id) override;

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

  VersionUpdater* GetVersionUpdaterForTesting();

  // Set a base clock (used to set current time) for testing EOL.
  void SetClockForTesting(base::Clock* clock);

 private:
  void EnsureScreenIsShown();

  void OnSelectNetworkButtonClicked();
  void OnUpdateButtonClicked();

  // NetworkStateHandlerObserver:
  void NetworkConnectionStateChanged(const NetworkState* network) override;
  void DefaultNetworkChanged(const NetworkState* network) override;

  void RefreshNetworkState();
  void RefreshView(const VersionUpdater::UpdateInfo& update_info);

  // Subscribes to network change notifications.
  void SubscribeNetworkNotification();

  // Unsubscribes from network change notifications.
  void UnsubscribeNetworkNotification();

  void HideErrorMessage();

  // The user requested an attempt to connect to the network should be made.
  void OnConnectRequested();

  void OnGetEolInfo(const chromeos::UpdateEngineClient::EolInfo& info);

  void OnErrorScreenHidden();

  // True if there was no notification about captive portal state for
  // the default network.
  bool is_first_portal_notification_ = true;

  UpdateRequiredView* view_ = nullptr;
  ErrorScreen* error_screen_;
  std::unique_ptr<ErrorScreensHistogramHelper> histogram_helper_;

  // Whether the screen is shown.
  bool is_shown_ = false;

  // True if subscribed to network change notification.
  bool is_network_subscribed_ = false;

  // True if Show() has never been called yet.
  bool first_time_shown_ = true;
  bool is_updating_now_ = false;
  bool waiting_for_reboot_ = false;
  bool waiting_for_permission_ = false;

  std::unique_ptr<VersionUpdater> version_updater_;
  std::unique_ptr<login::NetworkStateHelper> network_state_helper_;

  // Timer for the captive portal detector to show portal login page.
  // If redirect did not happen during this delay, error message is shown
  // instead.
  base::OneShotTimer error_message_timer_;

  // Overridden for testing EOL by setting the current time.
  base::Clock* clock_;

  ErrorScreen::ConnectRequestCallbackSubscription connect_request_subscription_;

  base::WeakPtrFactory<UpdateRequiredScreen> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(UpdateRequiredScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_UPDATE_REQUIRED_SCREEN_H_
