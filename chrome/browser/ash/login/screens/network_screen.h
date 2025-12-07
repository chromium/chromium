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
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/login/oobe_quick_start/target_device_bootstrap_controller.h"
#include "chrome/browser/ash/login/quickstart_controller.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-shared.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

class NetworkScreenView;
class NetworkStateHandler;

namespace login {
class NetworkStateHelper;
}

// Controls network selection screen shown during OOBE.
class NetworkScreen : public BaseScreen,
                      public NetworkStateHandlerObserver,
                      public quick_start::QuickStartController::UiDelegate {
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

  void set_no_quickstart_delay_for_testing() {
    quickstart_stabilization_period_ = base::Seconds(0);
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
  FRIEND_TEST_ALL_PREFIXES(NetworkScreenUnitTest, ContinuesOnUserAction);

  // BaseScreen:
  bool MaybeSkip(WizardContext& context) override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;
  bool HandleAccelerator(LoginAcceleratorAction action) override;

  // NetworkStateHandlerObserver:
  void NetworkConnectionStateChanged(const NetworkState* network) override;
  void DefaultNetworkChanged(const NetworkState* network) override;

  // quick_start::QuickStartController::UiDelegate:
  void OnUiUpdateRequested(
      quick_start::QuickStartController::UiState state) final;

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

  // Does an async call to add WiFi network with given credentials collected
  // from the Quick Start process.
  void ConfigureWifiNetwork(
      const quick_start::mojom::WifiCredentials& wifi_credentials);

  // Callback of AddWifiNetworkFromQuickStart async call.
  void OnConfigureWifiNetworkResult(
      const std::optional<std::string>& network_guid,
      const std::string& error_message);

  void OnStartConnectCompleted(
      chromeos::network_config::mojom::StartConnectResult result,
      const std::string& message);

  void ExitQuickStartFlow(
      quick_start::QuickStartController::AbortFlowReason reason);
  void ShowStepsWhenQuickStartOngoing();

  // Skip this screen or automatically continue if the device is connected to
  // Ethernet for the first time in this session.
  bool UpdateStatusIfConnectedToEthernet();

  // True if subscribed to network change notification.
  bool is_network_subscribed_ = false;

  // ID of the network that we are waiting for.
  std::u16string network_id_;

  // True if the user pressed the continue button in the UI.
  // Indicates that we should proceed with OOBE as soon as we are connected.
  bool continue_pressed_ = false;

  // Indicates whether the device has already been connected to Ethernet in this
  // session or not.
  bool first_ethernet_connection_ = true;

  // Whether wifi credentials were automatically received via Quick Start.
  bool did_receive_quickstart_wifi_credentials_ = false;

  // Whether the network screen is waiting the QuickStart stabilization period
  // before proceeding to the next screen.
  bool waiting_for_quickstart_stabilization_period_ = false;

  // Whether the QuickStart entry point visibility has already been determined.
  // This flag prevents duplicate histogram entries.
  bool has_emitted_quick_start_visible = false;

  // Default period to wait when going through QuickStart. Overridden in tests.
  base::TimeDelta quickstart_stabilization_period_ = base::Seconds(2);

  // Timer for connection timeout.
  base::OneShotTimer connection_timer_;

  base::WeakPtr<NetworkScreenView> view_;
  ScreenExitCallback exit_callback_;
  std::unique_ptr<login::NetworkStateHelper> network_state_helper_;

  base::ScopedObservation<NetworkStateHandler, NetworkStateHandlerObserver>
      network_state_handler_observer_{this};

  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      remote_cros_network_config_;

  base::WeakPtrFactory<NetworkScreen> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_NETWORK_SCREEN_H_
