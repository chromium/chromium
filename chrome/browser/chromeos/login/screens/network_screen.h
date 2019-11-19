// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_NETWORK_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_NETWORK_SCREEN_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "chromeos/network/network_state_handler_observer.h"

namespace chromeos {

class NetworkScreenView;
class ScreenManager;

namespace login {
class NetworkStateHelper;
}  // namespace login

// Controls network selection screen shown during OOBE.
class NetworkScreen : public BaseScreen, public NetworkStateHandlerObserver {
 public:
  enum class Result { CONNECTED, OFFLINE_DEMO_SETUP, BACK };

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;
  NetworkScreen(NetworkScreenView* view,
                const ScreenExitCallback& exit_callback);
  ~NetworkScreen() override;

  // Returns instance of NetworkScreen.
  static NetworkScreen* Get(ScreenManager* manager);

  // Called when |view| has been destroyed. If this instance is destroyed before
  // the |view| it should call view->Unbind().
  void OnViewDestroyed(NetworkScreenView* view);

  void set_exit_callback_for_testing(const ScreenExitCallback& exit_callback) {
    exit_callback_ = exit_callback;
  }

 protected:
  // Give test overrides access to the exit callback.
  ScreenExitCallback* exit_callback() { return &exit_callback_; }

 private:
  friend class NetworkScreenTest;
  friend class NetworkScreenUnitTest;
  friend class DemoSetupTest;
  FRIEND_TEST_ALL_PREFIXES(NetworkScreenTest, CanConnect);
  FRIEND_TEST_ALL_PREFIXES(NetworkScreenTest, Timeout);
  FRIEND_TEST_ALL_PREFIXES(NetworkScreenUnitTest, ContinuesAutomatically);
  FRIEND_TEST_ALL_PREFIXES(NetworkScreenUnitTest, ContinuesOnlyOnce);

  // BaseScreen:
  void Show() override;
  void Hide() override;
  void OnUserAction(const std::string& action_id) override;

  // NetworkStateHandlerObserver:
  void NetworkConnectionStateChanged(const NetworkState* network) override;
  void DefaultNetworkChanged(const NetworkState* network) override;

  // Subscribes NetworkScreen to the network change notification, forces refresh
  // of current network state.
  void Refresh();

  // Sets the NetworkStateHelper for use in tests. This class will take
  // ownership of the pointed object.
  void SetNetworkStateHelperForTest(login::NetworkStateHelper* helper);

  // Subscribes to network change notifications.
  void SubscribeNetworkNotification();

  // Unsubscribes from network change notifications.
  void UnsubscribeNetworkNotification();

  // Notifies wizard on successful connection.
  void NotifyOnConnection();

  // Called by |connection_timer_| when connection to the network timed out.
  void OnConnectionTimeout();

  // Updates UI based on current network status.
  void UpdateStatus();

  // Stops waiting for network to connect.
  void StopWaitingForConnection(const base::string16& network_id);

  // Starts waiting for network connection. Shows spinner.
  void WaitForConnection(const base::string16& network_id);

  // Called when back button is clicked.
  void OnBackButtonClicked();

  // Called when continue button is clicked.
  void OnContinueButtonClicked();

  // Called when the preinstalled demo resources check has completed.
  void OnHasPreinstalledDemoResources(bool has_preinstalled_demo_resources);

  // Called when offline demo mode setup was selected.
  void OnOfflineDemoModeSetupSelected();

  // True if subscribed to network change notification.
  bool is_network_subscribed_ = false;

  // ID of the network that we are waiting for.
  base::string16 network_id_;

  // Keeps track of the number of times OnContinueButtonClicked was called.
  // OnContinueButtonClicked is called either in response to the user pressing
  // the continue button, or automatically during hands-off enrollment after a
  // network connection is established.
  int continue_attempts_ = 0;

  // True if the user pressed the continue button in the UI.
  // Indicates that we should proceed with OOBE as soon as we are connected.
  bool continue_pressed_ = false;

  // Timer for connection timeout.
  base::OneShotTimer connection_timer_;

  NetworkScreenView* view_ = nullptr;
  ScreenExitCallback exit_callback_;
  std::unique_ptr<login::NetworkStateHelper> network_state_helper_;

  base::WeakPtrFactory<NetworkScreen> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NetworkScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_NETWORK_SCREEN_H_
