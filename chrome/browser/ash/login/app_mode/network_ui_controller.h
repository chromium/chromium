// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_APP_MODE_NETWORK_UI_CONTROLLER_H_
#define CHROME_BROWSER_ASH_LOGIN_APP_MODE_NETWORK_UI_CONTROLLER_H_

#include "base/timer/timer.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launcher.h"
#include "chrome/browser/ui/webui/ash/login/app_launch_splash_screen_handler.h"

class Profile;

namespace ash {

class LoginDisplayHost;

class NetworkUiController : public AppLaunchSplashScreenView::Delegate,
                            public KioskAppLauncher::NetworkDelegate {
 public:
  enum NetworkUIState {
    kNotShowing = 0,     // Network configure UI is not being shown.
    kNeedToShow,         // We need to show the UI as soon as we can.
    kShowing,            // Network configure UI is being shown.
    kWaitingForNetwork,  // App requested network and we're waiting for it
  };

  class Observer {
   public:
    Observer() = default;
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;
    ~Observer() = default;

    virtual void OnNetworkConfigureUiShowing() = 0;
    virtual void OnNetworkConfigureUiFinished() = 0;
    virtual void OnNetworkReady() = 0;
    virtual void OnNetworkLost() = 0;
  };

  NetworkUiController(Observer& observer,
                      LoginDisplayHost* host,
                      AppLaunchSplashScreenView* splash_screen);
  NetworkUiController(const NetworkUiController&) = delete;
  NetworkUiController& operator=(const NetworkUiController&) = delete;
  ~NetworkUiController() override;

  void SetProfile(Profile* profile);
  void UserRequestedNetworkConfig();
  bool ShouldShowNetworkConfig();
  void OnNetworkLostDuringInstallation();

  // `AppLaunchSplashScreenView::Delegate`
  void OnConfigureNetwork() override;
  void OnNetworkConfigFinished() override;
  void OnNetworkStateChanged(bool online) override;

  // `KioskAppLauncher::NetworkDelegate`
  void InitializeNetwork() override;
  bool IsNetworkReady() const override;

  NetworkUIState GetNetworkUiStateForTesting() const {
    return network_ui_state_;
  }

  static void SetCanConfigureNetworkCallbackForTesting(
      base::RepeatingCallback<bool()>* callback);

 private:
  void MaybeShowNetworkConfigureUI();
  void MaybeShowNetworkConfigureUIForConsumerKiosk();
  void ShowNetworkConfigureUI();
  void CloseNetworkConfigureUI();

  void OnNetworkWaitTimeout();
  bool CanConfigureNetwork();

  void OnNetworkOnline();
  void OnNetworkOffline();

  const raw_ref<Observer> observer_;
  const raw_ptr<LoginDisplayHost> host_;
  const raw_ptr<AppLaunchSplashScreenView> splash_screen_view_;
  raw_ptr<Profile> profile_ = nullptr;

  NetworkUIState network_ui_state_ = kNotShowing;
  bool network_required_ = false;

  // A timer that fires when the network was not prepared and we require user
  // network configuration to continue.
  base::OneShotTimer network_wait_timer_;
  bool network_wait_timeout_ = false;

  base::WeakPtrFactory<NetworkUiController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_APP_MODE_NETWORK_UI_CONTROLLER_H_
