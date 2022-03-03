// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_NETWORK_HEALTH_NETWORK_HEALTH_H_
#define CHROME_BROWSER_ASH_NET_NETWORK_HEALTH_NETWORK_HEALTH_H_

#include <memory>
#include <string>
#include <vector>

#include "base/timer/timer.h"
#include "chrome/browser/ash/net/network_health/signal_strength_tracker.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "chromeos/services/network_health/public/mojom/network_health.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash {
namespace network_health {

class NetworkHealth
    : public chromeos::network_health::mojom::NetworkHealthService,
      public chromeos::network_config::mojom::CrosNetworkConfigObserver {
 public:
  NetworkHealth();

  ~NetworkHealth() override;

  // Binds this instance to |receiver|.
  void BindReceiver(
      mojo::PendingReceiver<
          chromeos::network_health::mojom::NetworkHealthService> receiver);

  // Returns the current NetworkHealthState.
  const chromeos::network_health::mojom::NetworkHealthStatePtr
  GetNetworkHealthState();

  // NetworkHealthService
  void AddObserver(mojo::PendingRemote<
                   chromeos::network_health::mojom::NetworkEventsObserver>
                       observer) override;
  void GetNetworkList(GetNetworkListCallback) override;
  void GetHealthSnapshot(GetHealthSnapshotCallback) override;

  // CrosNetworkConfigObserver
  void OnNetworkStateListChanged() override;
  void OnDeviceStateListChanged() override;
  void OnActiveNetworksChanged(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          active_networks) override;
  void OnNetworkStateChanged(
      chromeos::network_config::mojom::NetworkStatePropertiesPtr network_state)
      override;
  void OnVpnProvidersChanged() override {}
  void OnNetworkCertificatesChanged() override {}
  void OnPoliciesApplied(const std::string& userhash) override {}

  // Signal strength changes larger than
  // |kMaxSignalStrengthFluctuationTolerance| trigger a signal strength change
  // event.
  static constexpr int kMaxSignalStrengthFluctuationTolerance = 10;

 protected:
  // Used to set the internal timer. Can be called by derived classes for
  // testing.
  void SetTimer(std::unique_ptr<base::RepeatingTimer> timer);

 private:
  // Handler for receiving the network state list.
  void OnNetworkStateListReceived(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>);

  // Handler for receiving networking devices.
  void OnDeviceStateListReceived(
      std::vector<chromeos::network_config::mojom::DeviceStatePropertiesPtr>);

  // Creates the NetworkHealthState structure from cached network information.
  void CreateNetworkHealthState();

  // Asynchronous call that refreshes the current Network Health State.
  void RefreshNetworkHealthState();
  void RequestNetworkStateList();
  void RequestDeviceStateList();

  // Finds the matching network using |guid|.
  const chromeos::network_health::mojom::NetworkPtr* FindMatchingNetwork(
      const std::string& guid) const;

  // Handles the case when an active network changes. Also handles the case
  // when a network that was not active becomes active.
  void HandleNetworkEventsForActiveNetworks(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          active_networks);

  // Handles the case when an active network becomes no longer active.
  void HandleNetworkEventsForInactiveNetworks(
      chromeos::network_config::mojom::NetworkStatePropertiesPtr network);

  // Notifies observers of connection state changes.
  void NotifyObserversConnectionStateChanged(
      const std::string& guid,
      chromeos::network_health::mojom::NetworkState state);

  // Notifies observers of signal strength changes.
  void NotifyObserversSignalStrengthChanged(const std::string& guid,
                                            int signal_strength);

  // Checks if a connection state changed has occurred.
  bool ConnectionStateChanged(
      const chromeos::network_health::mojom::NetworkPtr& network,
      const chromeos::network_config::mojom::NetworkStatePropertiesPtr&
          network_state);

  // Checks if a signal strength change event has occurred.
  bool SignalStrengthChanged(
      const chromeos::network_health::mojom::NetworkPtr& network,
      const chromeos::network_config::mojom::NetworkStatePropertiesPtr&
          network_state);

  // Function to add a signal strength sample for each network and update the
  // statistics over time for each network.
  void AnalyzeSignalStrength();

  // Remotes for tracking observers that will be notified of network events in
  // the mojom::NetworkEventsObserver interface.
  mojo::RemoteSet<chromeos::network_health::mojom::NetworkEventsObserver>
      observers_;
  // Remote for sending requests to the CrosNetworkConfig service.
  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      remote_cros_network_config_;
  // Receiver for the CrosNetworkConfigObserver events.
  mojo::Receiver<chromeos::network_config::mojom::CrosNetworkConfigObserver>
      cros_network_config_observer_receiver_{this};
  // Receivers for external requests (WebUI, Feedback, CrosHealthdClient).
  mojo::ReceiverSet<chromeos::network_health::mojom::NetworkHealthService>
      receivers_;

  // Container for storing a running tally of the average signal strength per
  // network GUID.
  std::map<std::string, SignalStrengthTracker> signal_strength_trackers_;
  // Timer that triggers the function to analyze the networks' signal strengths.
  std::unique_ptr<base::RepeatingTimer> timer_;

  chromeos::network_health::mojom::NetworkHealthState network_health_state_;
  std::vector<chromeos::network_config::mojom::DeviceStatePropertiesPtr>
      device_properties_;
  std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
      network_properties_;
};

}  // namespace network_health
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NET_NETWORK_HEALTH_NETWORK_HEALTH_H_
