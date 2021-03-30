// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_health/network_health.h"

#include <cstdint>
#include <cstdlib>
#include <map>
#include <utility>
#include <vector>

#include "chromeos/network/network_event_log.h"
#include "chromeos/services/network_config/in_process_instance.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "chromeos/services/network_health/public/mojom/network_health.mojom.h"

namespace chromeos {
namespace network_health {

namespace {

constexpr mojom::NetworkState DeviceStateToNetworkState(
    network_config::mojom::DeviceStateType device_state) {
  switch (device_state) {
    case network_config::mojom::DeviceStateType::kUninitialized:
      return mojom::NetworkState::kUninitialized;
    case network_config::mojom::DeviceStateType::kDisabled:
    case network_config::mojom::DeviceStateType::kDisabling:
    case network_config::mojom::DeviceStateType::kEnabling:
      // Disabling and Enabling are intermediate state that we care about in the
      // UI, but not for purposes of network health, we can treat as Disabled.
      return mojom::NetworkState::kDisabled;
    case network_config::mojom::DeviceStateType::kEnabled:
      return mojom::NetworkState::kNotConnected;
    case network_config::mojom::DeviceStateType::kProhibited:
      return mojom::NetworkState::kProhibited;
    case network_config::mojom::DeviceStateType::kUnavailable:
      NOTREACHED();
      return mojom::NetworkState::kUninitialized;
  }
}

constexpr mojom::NetworkState ConnectionStateToNetworkState(
    network_config::mojom::ConnectionStateType connection_state) {
  switch (connection_state) {
    case network_config::mojom::ConnectionStateType::kOnline:
      return mojom::NetworkState::kOnline;
    case network_config::mojom::ConnectionStateType::kConnected:
      return mojom::NetworkState::kConnected;
    case network_config::mojom::ConnectionStateType::kPortal:
      return mojom::NetworkState::kPortal;
    case network_config::mojom::ConnectionStateType::kConnecting:
      return mojom::NetworkState::kConnecting;
    case network_config::mojom::ConnectionStateType::kNotConnected:
      return mojom::NetworkState::kNotConnected;
  }
}

// Populates a mojom::NetworkPtr based on the given |device_prop| and
// |network_prop|. This function assumes that |device_prop| is populated, while
// |network_prop| could be null.
mojom::NetworkPtr CreateNetwork(
    const network_config::mojom::DeviceStatePropertiesPtr& device_prop,
    const network_config::mojom::NetworkStatePropertiesPtr& net_prop) {
  auto net = mojom::Network::New();
  net->mac_address = device_prop->mac_address;
  net->type = device_prop->type;
  if (device_prop->ipv6_address)
    net->ipv6_addresses.push_back(device_prop->ipv6_address->ToString());
  if (device_prop->ipv4_address)
    net->ipv4_address = device_prop->ipv4_address->ToString();

  if (net_prop) {
    net->state = ConnectionStateToNetworkState(net_prop->connection_state);
    net->name = net_prop->name;
    net->guid = net_prop->guid;
    net->portal_state = net_prop->portal_state;
    if (chromeos::network_config::NetworkTypeMatchesType(
            net_prop->type, network_config::mojom::NetworkType::kWireless)) {
      net->signal_strength = network_health::mojom::UInt32Value::New(
          network_config::GetWirelessSignalStrength(net_prop.get()));
    }
  } else {
    net->state = DeviceStateToNetworkState(device_prop->device_state);
  }

  return net;
}

}  // namespace

NetworkHealth::NetworkHealth() {
  network_config::BindToInProcessInstance(
      remote_cros_network_config_.BindNewPipeAndPassReceiver());
  remote_cros_network_config_->AddObserver(
      cros_network_config_observer_receiver_.BindNewPipeAndPassRemote());
  RefreshNetworkHealthState();
}

NetworkHealth::~NetworkHealth() = default;

void NetworkHealth::BindReceiver(
    mojo::PendingReceiver<mojom::NetworkHealthService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

const mojom::NetworkHealthStatePtr NetworkHealth::GetNetworkHealthState() {
  NET_LOG(EVENT) << "Network Health State Requested";
  return network_health_state_.Clone();
}

void NetworkHealth::AddObserver(
    mojo::PendingRemote<mojom::NetworkEventsObserver> observer) {
  observers_.Add(std::move(observer));
}

void NetworkHealth::GetNetworkList(GetNetworkListCallback callback) {
  std::move(callback).Run(mojo::Clone(network_health_state_.networks));
}

void NetworkHealth::GetHealthSnapshot(GetHealthSnapshotCallback callback) {
  std::move(callback).Run(network_health_state_.Clone());
}

void NetworkHealth::OnNetworkStateListChanged() {
  RequestNetworkStateList();
}

void NetworkHealth::OnDeviceStateListChanged() {
  RequestDeviceStateList();
}

void NetworkHealth::OnActiveNetworksChanged(
    std::vector<network_config::mojom::NetworkStatePropertiesPtr>
        active_networks) {
  HandleNetworkEventsForActiveNetworks(std::move(active_networks));
}

void NetworkHealth::OnNetworkStateChanged(
    network_config::mojom::NetworkStatePropertiesPtr network_state) {
  if (!network_state) {
    return;
  }
  HandleNetworkEventsForInactiveNetworks(std::move(network_state));
}

void NetworkHealth::OnVpnProvidersChanged() {}

void NetworkHealth::OnNetworkCertificatesChanged() {}

void NetworkHealth::OnNetworkStateListReceived(
    std::vector<network_config::mojom::NetworkStatePropertiesPtr> props) {
  network_properties_.swap(props);
  CreateNetworkHealthState();
}

void NetworkHealth::OnDeviceStateListReceived(
    std::vector<network_config::mojom::DeviceStatePropertiesPtr> props) {
  device_properties_.swap(props);
  CreateNetworkHealthState();
}

void NetworkHealth::CreateNetworkHealthState() {
  // If the device information has not been collected, the NetworkHealthState
  // cannot be created.
  if (device_properties_.empty())
    return;

  network_health_state_.networks.clear();

  std::map<network_config::mojom::NetworkType,
           network_config::mojom::DeviceStatePropertiesPtr>
      device_type_map;

  // This function only supports one Network structure per underlying device. If
  // this assumption changes, this function will need to be reworked.
  for (const auto& d : device_properties_) {
    device_type_map[d->type] = mojo::Clone(d);
  }

  // For each NetworkStateProperties, create a Network structure using the
  // underlying DeviceStateProperties. Remove devices from the type map that
  // have an associated NetworkStateProperties.
  for (const auto& net_prop : network_properties_) {
    auto device_iter = device_type_map.find(net_prop->type);
    if (device_iter == device_type_map.end()) {
      continue;
    }
    network_health_state_.networks.push_back(
        CreateNetwork(device_iter->second, net_prop));
    device_type_map.erase(device_iter);
  }

  // For the remaining devices that do not have associated
  // NetworkStateProperties, create Network structures.
  for (const auto& device_prop : device_type_map) {
    // Devices that have an kUnavailable state are not valid.
    if (device_prop.second->device_state ==
        network_config::mojom::DeviceStateType::kUnavailable) {
      NET_LOG(ERROR) << "Device in unexpected unavailable state: "
                     << device_prop.second->type;
      continue;
    }

    network_health_state_.networks.push_back(
        CreateNetwork(device_prop.second, nullptr));
  }
}

void NetworkHealth::RefreshNetworkHealthState() {
  RequestNetworkStateList();
  RequestDeviceStateList();
}

void NetworkHealth::RequestNetworkStateList() {
  remote_cros_network_config_->GetNetworkStateList(
      network_config::mojom::NetworkFilter::New(
          network_config::mojom::FilterType::kActive,
          network_config::mojom::NetworkType::kAll,
          network_config::mojom::kNoLimit),
      base::BindOnce(&NetworkHealth::OnNetworkStateListReceived,
                     base::Unretained(this)));
}

void NetworkHealth::RequestDeviceStateList() {
  remote_cros_network_config_->GetDeviceStateList(base::BindOnce(
      &NetworkHealth::OnDeviceStateListReceived, base::Unretained(this)));
}

const mojom::NetworkPtr* NetworkHealth::FindMatchingNetwork(
    const std::string& guid) const {
  for (const mojom::NetworkPtr& network : network_health_state_.networks) {
    if (!network->guid) {
      continue;
    }
    if (network->guid.value() == guid) {
      return &network;
    }
  }
  return nullptr;
}

void NetworkHealth::HandleNetworkEventsForActiveNetworks(
    std::vector<network_config::mojom::NetworkStatePropertiesPtr>
        active_networks) {
  bool refresh_network_health_state = false;
  for (const auto& network_state : active_networks) {
    const mojom::NetworkPtr* const network =
        FindMatchingNetwork(network_state->guid);
    // Fire an event if the network is new or a connection state change
    // occurred.
    if (!network || ConnectionStateChanged(*network, network_state)) {
      refresh_network_health_state = true;
      NotifyObserversConnectionStateChanged(
          network_state->guid,
          ConnectionStateToNetworkState(network_state->connection_state));
    }
    // Fire an event if the network is new or a signal strength change occurred.
    if (!network || SignalStrengthChanged(*network, network_state)) {
      refresh_network_health_state = true;
      NotifyObserversSignalStrengthChanged(
          network_state->guid,
          network_config::GetWirelessSignalStrength(network_state.get()));
    }
  }
  if (refresh_network_health_state) {
    RefreshNetworkHealthState();
  }
}

void NetworkHealth::HandleNetworkEventsForInactiveNetworks(
    network_config::mojom::NetworkStatePropertiesPtr network_state) {
  // Ensure that the connection state is no longer active.
  if (ConnectionStateToNetworkState(network_state->connection_state) !=
      mojom::NetworkState::kNotConnected) {
    return;
  }
  const mojom::NetworkPtr* const network =
      FindMatchingNetwork(network_state->guid);
  if (!network) {
    return;
  }
  // Fire an event if the network was previously active.
  if (ConnectionStateChanged(*network, network_state)) {
    NotifyObserversConnectionStateChanged(
        network_state->guid,
        ConnectionStateToNetworkState(network_state->connection_state));
    RefreshNetworkHealthState();
  }
}

void NetworkHealth::NotifyObserversConnectionStateChanged(
    const std::string& guid,
    mojom::NetworkState state) {
  for (auto& observer : observers_) {
    observer->OnConnectionStateChanged(guid, state);
  }
}

void NetworkHealth::NotifyObserversSignalStrengthChanged(
    const std::string& guid,
    int signal_strength) {
  for (auto& observer : observers_) {
    observer->OnSignalStrengthChanged(guid,
                                      mojom::UInt32Value::New(signal_strength));
  }
}

bool NetworkHealth::ConnectionStateChanged(
    const mojom::NetworkPtr& network,
    const network_config::mojom::NetworkStatePropertiesPtr& network_state) {
  auto state = ConnectionStateToNetworkState(network_state->connection_state);
  if (state == network->state) {
    return false;
  }
  return true;
}

bool NetworkHealth::SignalStrengthChanged(
    const mojom::NetworkPtr& network,
    const network_config::mojom::NetworkStatePropertiesPtr& network_state) {
  if (!network_config::NetworkStateMatchesType(
          network_state.get(), network_config::mojom::NetworkType::kWireless)) {
    return false;
  }
  DCHECK(network->signal_strength);

  auto current = network_config::GetWirelessSignalStrength(network_state.get());
  uint32_t previous = network->signal_strength->value;
  if (std::abs(1.0 * (current - (int)previous)) <=
      kMaxSignalStrengthFluctuationTolerance) {
    return false;
  }
  return true;
}

}  // namespace network_health
}  // namespace chromeos
