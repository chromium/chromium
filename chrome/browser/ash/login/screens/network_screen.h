// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_NETWORK_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_NETWORK_SCREEN_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/login/oobe_quick_start/target_device_bootstrap_controller.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"

namespace ash {

class NetworkScreenView;
class NetworkStateHandler;

namespace login {
class NetworkStateHelper;
}

// Controls network selection screen shown during OOBE.
class NetworkScreen : public BaseScreen, public NetworkStateHandlerObserver {
 public:
  using TView = NetworkScreenView;

  enum class Result {
    CONNECTED,
    BACK,
    QUICK_START,
    NOT_APPLICABLE,
  };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  NetworkScreen(base::WeakPtr<NetworkScreenView> view,
                const ScreenExitCallback& exit_callback);

  NetworkScreen(const NetworkScreen&) = delete;
  NetworkScreen& operator=(const NetworkScreen&) = delete;

  ~NetworkScreen() override;

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
  FRIEND_TEST_ALL_PREFIXES(NetworkScreenTest, HandsOffCanConnect_Skipped);
  FRIEND_TEST_ALL_PREFIXES(NetworkScreenTest, HandsOffTimeout_NotSkipped);
  FRIEND_TEST_ALL_PREFIXES(NetworkScreenTest,
                           DelayedEthernetConnection_Skipped);
  FRIEND_TEST_ALL_PREFIXES(NetworkScreenUnitTest, ContinuesAutomatically);
  FRIEND_TEST_ALL_PREFIXES(NetworkScreenUnitTest, ContinuesOnlyOnce);

  // BaseScreen:
  bool MaybeSkip(WizardContext& context) override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;
  bool HandleAccelerator(LoginAcceleratorAction action) override;

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

  // Called by `connection_timer_` when connection to the network timed out.
  void OnConnectionTimeout();

  // Updates UI based on current network status.
  void UpdateStatus();

  // Stops waiting for network to connect.
  void StopWaitingForConnection(const std::u16string& network_id);

  // Starts waiting for network connection. Shows spinner.
  void WaitForConnection(const std::u16string& network_id);

  // Called when back button is clicked.
  void OnBackButtonClicked();

  // Called when continue button is clicked.
  void OnContinueButtonClicked();

  // Called when quick start button is clicked.
  void OnQuickStartButtonClicked();
  void SetQuickStartButtonVisibility(bool visible);

  // Skip this screen or automatically continue if the device is connected to
  // Ethernet for the first time in this session.
  bool UpdateStatusIfConnectedToEthernet();

  // True if subscribed to network change notification.
  bool is_network_subscribed_ = false;

  // ID of the network that we are waiting for.
  std::u16string network_id_;

  // Keeps track of the number of times OnContinueButtonClicked was called.
  // OnContinueButtonClicked is called either in response to the user pressing
  // the continue button, or automatically during hands-off enrollment after a
  // network connection is established.
  int continue_attempts_ = 0;

  // True if the user pressed the continue button in the UI.
  // Indicates that we should proceed with OOBE as soon as we are connected.
  bool continue_pressed_ = false;

  // Indicates whether the device has already been connected to Ethernet in this
  // session or not.
  bool first_ethernet_connection_ = true;

  // Timer for connection timeout.
  base::OneShotTimer connection_timer_;

  base::WeakPtr<NetworkScreenView> view_;
  ScreenExitCallback exit_callback_;
  std::unique_ptr<login::NetworkStateHelper> network_state_helper_;

  base::ScopedObservation<NetworkStateHandler, NetworkStateHandlerObserver>
      network_state_handler_observer_{this};

  base::WeakPtrFactory<NetworkScreen> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_NETWORK_SCREEN_H_
