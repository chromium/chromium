// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/network_health_provider.h"

#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/containers/contains.h"
#include "chromeos/services/network_config/in_process_instance.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"

namespace ash {
namespace diagnostics {
namespace {

namespace network_mojom = ::chromeos::network_config::mojom;

bool IsSupportedNetworkType(network_mojom::NetworkType type) {
  switch (type) {
    case network_mojom::NetworkType::kWiFi:
    case network_mojom::NetworkType::kCellular:
    case network_mojom::NetworkType::kEthernet:
      return true;
    case network_mojom::NetworkType::kMobile:
    case network_mojom::NetworkType::kTether:
    case network_mojom::NetworkType::kVPN:
    case network_mojom::NetworkType::kAll:
    case network_mojom::NetworkType::kWireless:
      return false;
  }
}

bool IsConnectedOrConnecting(
    network_mojom::ConnectionStateType connection_state) {
  switch (connection_state) {
    case network_mojom::ConnectionStateType::kOnline:
    case network_mojom::ConnectionStateType::kConnected:
    case network_mojom::ConnectionStateType::kPortal:
    case network_mojom::ConnectionStateType::kConnecting:
      return true;
    case network_mojom::ConnectionStateType::kNotConnected:
      return false;
  }
}

constexpr mojom::NetworkState ConnectionStateToNetworkState(
    network_mojom::ConnectionStateType connection_state) {
  switch (connection_state) {
    case network_mojom::ConnectionStateType::kOnline:
      return mojom::NetworkState::kOnline;
    case network_mojom::ConnectionStateType::kConnected:
      return mojom::NetworkState::kConnected;
    case network_mojom::ConnectionStateType::kPortal:
      return mojom::NetworkState::kPortal;
    case network_mojom::ConnectionStateType::kConnecting:
      return mojom::NetworkState::kConnecting;
    case network_mojom::ConnectionStateType::kNotConnected:
      return mojom::NetworkState::kNotConnected;
  }
}

constexpr mojom::NetworkType ConvertNetworkType(
    network_mojom::NetworkType type) {
  switch (type) {
    case network_mojom::NetworkType::kWiFi:
      return mojom::NetworkType::kWiFi;
    case network_mojom::NetworkType::kCellular:
      return mojom::NetworkType::kCellular;
    case network_mojom::NetworkType::kEthernet:
      return mojom::NetworkType::kEthernet;
    case network_mojom::NetworkType::kMobile:
    case network_mojom::NetworkType::kTether:
    case network_mojom::NetworkType::kVPN:
    case network_mojom::NetworkType::kAll:
    case network_mojom::NetworkType::kWireless:
      return mojom::NetworkType::kUnsupported;
  }
}

mojom::IPConfigPropertiesPtr CreateIPConfigProperties(
    const network_mojom::IPConfigPropertiesPtr& ip_config_props) {
  mojom::IPConfigPropertiesPtr ip_config = mojom::IPConfigProperties::New();
  ip_config->ip_address = ip_config_props->ip_address;
  ip_config->routing_prefix = ip_config_props->routing_prefix;
  ip_config->gateway = ip_config_props->gateway;
  ip_config->name_servers = ip_config_props->name_servers;
  return ip_config;
}

mojom::WiFiStatePropertiesPtr CreateWiFiStateProperties(
    const network_mojom::NetworkTypeStateProperties& network_type_props) {
  auto wifi_props = mojom::WiFiStateProperties::New();
  wifi_props->signal_strength = network_type_props.get_wifi()->signal_strength;
  wifi_props->frequency = network_type_props.get_wifi()->frequency;
  wifi_props->ssid = network_type_props.get_wifi()->ssid;
  wifi_props->bssid = network_type_props.get_wifi()->bssid;
  return wifi_props;
}

// TODO(michaelcheco): Add Ethernet properties.
mojom::EthernetStatePropertiesPtr CreateEthernetStateProperties(
    const network_mojom::NetworkTypeStateProperties& network_type_props) {
  return mojom::EthernetStateProperties::New();
}

// TODO(michaelcheco): Add Cellular properties.
mojom::CellularStatePropertiesPtr CreateCellularStateProperties(
    const network_mojom::NetworkTypeStateProperties& network_type_props) {
  return mojom::CellularStateProperties::New();
}

bool IsMatchingDevice(const network_mojom::DeviceStatePropertiesPtr& device,
                      const mojom::NetworkPtr& network) {
  return ConvertNetworkType(device->type) == network->type;
}

// When |must_match_existing_guid| is true a network will only match if the
// backend network guid matches. This is to perform state updates to already
// known networks. When false, the network will match to a device/interface
// with the same type which allows rebinding a new network to a
// device/interface.
bool IsMatchingNetwork(
    const network_mojom::NetworkStatePropertiesPtr& backend_network,
    const NetworkObserverInfo& network_info,
    bool must_match_existing_guid) {
  const bool types_match =
      ConvertNetworkType(backend_network->type) == network_info.network->type;
  if (!must_match_existing_guid) {
    return types_match;
  }

  const std::string& network_guid = network_info.network_guid;
  if (IsConnectedOrConnecting(backend_network->connection_state)) {
    return types_match &&
           (network_guid.empty() || (network_guid == backend_network->guid));
  }

  return types_match && (network_guid == backend_network->guid);
}

bool ClearDisconnectedNetwork(NetworkObserverInfo* network_info) {
  mojom::Network* network = network_info->network.get();
  network_info->network_guid.clear();
  network->ip_config = nullptr;
  network->type_properties = nullptr;
  return true;
}

void UpdateNetwork(
    const network_mojom::NetworkTypeStateProperties& network_type_props,
    mojom::Network* network) {
  auto type_properties = mojom::NetworkTypeProperties::New();
  switch (network->type) {
    case mojom::NetworkType::kWiFi:
      type_properties->set_wifi(CreateWiFiStateProperties(network_type_props));
      break;
    case mojom::NetworkType::kEthernet:
      type_properties->set_ethernet(
          CreateEthernetStateProperties(network_type_props));
      break;
    case mojom::NetworkType::kCellular:
      type_properties->set_cellular(
          CreateCellularStateProperties(network_type_props));
      break;
    case mojom::NetworkType::kUnsupported:
      NOTREACHED();
      break;
  }

  network->type_properties = std::move(type_properties);
}

void UpdateNetwork(
    const network_mojom::NetworkStatePropertiesPtr& network_state,
    NetworkObserverInfo* network_info) {
  mojom::Network* network = network_info->network.get();
  network->name = network_state->name;
  network->state =
      ConnectionStateToNetworkState(network_state->connection_state);

  // Network type must be populated before calling UpdateNetwork.
  network->type = ConvertNetworkType(network_state->type);

  if (IsConnectedOrConnecting(network_state->connection_state)) {
    network_info->network_guid = network_state->guid;
    UpdateNetwork(*network_state->type_state, network);
    return;
  }

  ClearDisconnectedNetwork(network_info);
}

void UpdateNetwork(const network_mojom::DeviceStatePropertiesPtr& device,
                   NetworkObserverInfo* network_info) {
  mojom::Network* network = network_info->network.get();
  DCHECK(!network->guid.empty());

  network->type = ConvertNetworkType(device->type);
  network->mac_address = device->mac_address;

  // TODO(michaelcheco): Combine the applicable device states like
  // kDisabled and kDisabling into the combined NetworkState enum.
  if (device->device_state != network_mojom::DeviceStateType::kEnabled) {
    network->state = mojom::NetworkState::kNotConnected;
    ClearDisconnectedNetwork(network_info);
  }
}

void UpdateNetwork(
    const network_mojom::ManagedPropertiesPtr& managed_properties,
    NetworkObserverInfo* network_info) {
  DCHECK(network_info);
  mojom::Network* network = network_info->network.get();

  const bool has_ip_config =
      managed_properties && managed_properties->ip_configs.has_value();
  if (has_ip_config) {
    const int ip_configs_len = managed_properties->ip_configs.value().size();
    DCHECK(ip_configs_len >= 1);
    if (ip_configs_len > 1) {
      LOG(WARNING) << "More than one entry in ManagedProperties' ip_configs "
                      "array, selecting the first, IP config count is: "
                   << ip_configs_len;
    }

    // TODO(zentaro): Investigate IPV6.
    auto ip_config = managed_properties->ip_configs.value()[0].Clone();
    network->ip_config = CreateIPConfigProperties(ip_config);
  }
}

}  // namespace

NetworkObserverInfo::NetworkObserverInfo() = default;
NetworkObserverInfo::NetworkObserverInfo(NetworkObserverInfo&&) = default;
NetworkObserverInfo& NetworkObserverInfo::operator=(NetworkObserverInfo&&) =
    default;
NetworkObserverInfo::~NetworkObserverInfo() = default;

NetworkHealthProvider::NetworkHealthProvider()
    : NetworkHealthProvider(/*networking_log_ptr_=*/nullptr) {}

NetworkHealthProvider::NetworkHealthProvider(NetworkingLog* networking_log_ptr)
    : networking_log_ptr_(networking_log_ptr) {
  network_config::BindToInProcessInstance(
      remote_cros_network_config_.BindNewPipeAndPassReceiver());
  remote_cros_network_config_->AddObserver(
      cros_network_config_observer_receiver_.BindNewPipeAndPassRemote());

  // Get initial device/interface state.
  GetDeviceState();
}

NetworkHealthProvider::~NetworkHealthProvider() = default;

void NetworkHealthProvider::ObserveNetworkList(
    mojo::PendingRemote<mojom::NetworkListObserver> observer) {
  // Add the observer, then fire it immediately.
  network_list_observers_.Add(std::move(observer));
  NotifyNetworkListObservers();
}

void NetworkHealthProvider::ObserveNetwork(
    mojo::PendingRemote<mojom::NetworkStateObserver> observer,
    const std::string& observer_guid) {
  auto iter = networks_.find(observer_guid);
  if (iter == networks_.end()) {
    LOG(WARNING)
        << "Ignoring request to observe network that does not exist. guid="
        << observer_guid;
    return;
  }

  // Add the observer, then fire it immediately.
  iter->second.observer =
      mojo::Remote<mojom::NetworkStateObserver>(std::move(observer));
  NotifyNetworkStateObserver(iter->second);
}

void NetworkHealthProvider::OnNetworkStateListChanged() {}

void NetworkHealthProvider::OnDeviceStateListChanged() {
  GetDeviceState();
}

void NetworkHealthProvider::OnActiveNetworksChanged(
    std::vector<network_mojom::NetworkStatePropertiesPtr> active_networks) {
  OnActiveNetworkStateListReceived(std::move(active_networks));
}

void NetworkHealthProvider::OnNetworkStateChanged(
    network_mojom::NetworkStatePropertiesPtr network) {
  UpdateMatchingNetwork(std::move(network), /*must_match_existing_guid=*/true);
}

void NetworkHealthProvider::OnVpnProvidersChanged() {}
void NetworkHealthProvider::OnNetworkCertificatesChanged() {}

void NetworkHealthProvider::OnActiveNetworkStateListReceived(
    std::vector<network_mojom::NetworkStatePropertiesPtr> networks) {
  for (auto& network : networks) {
    UpdateMatchingNetwork(std::move(network),
                          /*must_match_existing_guid=*/false);
  }
}

void NetworkHealthProvider::UpdateMatchingNetwork(
    network_mojom::NetworkStatePropertiesPtr network,
    bool must_match_existing_guid) {
  NetworkObserverInfo* network_info =
      LookupNetwork(network, must_match_existing_guid);
  if (!network_info) {
    if (!must_match_existing_guid) {
      LOG(WARNING) << "Ignoring network " << network->guid
                   << " without matching interface. Network count is "
                   << networks_.size();
    }

    return;
  }

  UpdateNetwork(network, network_info);

  // Get the managed properties using the network guid, and pass the observer
  // guid.
  GetManagedPropertiesForNetwork(/*network_guid=*/network->guid,
                                 /*observer_guid=*/network_info->network->guid);
}

void NetworkHealthProvider::OnDeviceStateListReceived(
    std::vector<network_mojom::DeviceStatePropertiesPtr> devices) {
  bool list_changed = false;
  base::flat_set<std::string> networks_seen;

  // Iterate all devices. If the device is already known, then update it's
  // state, otherwise add the new device to networks_. Any device no longer
  // present in devices will be removed from networks_.
  for (auto& device : devices) {
    if (!IsSupportedNetworkType(device->type)) {
      continue;
    }

    // Find a matching entry in networks_ for device and update it if present.
    bool matched = false;
    for (auto& pair : networks_) {
      const std::string& observer_guid = pair.first;
      NetworkObserverInfo& info = pair.second;
      mojom::NetworkPtr& network = info.network;

      // If this device is already known, update it's state.
      if (IsMatchingDevice(device, network)) {
        networks_seen.insert(observer_guid);
        matched = true;
        UpdateNetwork(device, &info);
        break;
      }
    }

    // If a matching entry could not be found, add a new entry to |networks_|.
    if (!matched) {
      std::string guid = AddNewNetwork(device);
      networks_seen.insert(std::move(guid));
      list_changed = true;
    }
  }

  // Remove any entry in |networks_| that doesn't match a device.
  for (auto it = networks_.begin(); it != networks_.end();) {
    const std::string& guid = it->first;
    if (!base::Contains(networks_seen, guid)) {
      it = networks_.erase(it);
      list_changed = true;
      continue;
    }

    ++it;
  }

  // Trigger a request for the list of active networks to get state updates
  // to the current list of devices/interfaces.
  GetActiveNetworkState();

  // Update the currently active network and fire the network list observer.
  NotifyNetworkListObservers();
}

std::string NetworkHealthProvider::AddNewNetwork(
    const chromeos::network_config::mojom::DeviceStatePropertiesPtr& device) {
  std::string observer_guid = base::GenerateGUID();
  auto network = mojom::Network::New();
  network->guid = observer_guid;

  // Initial state set to kNotConnected.
  network->state = mojom::NetworkState::kNotConnected;

  NetworkObserverInfo info;
  info.network = std::move(network);
  UpdateNetwork(device, &info);

  networks_.emplace(observer_guid, std::move(info));
  return observer_guid;
}

NetworkObserverInfo* NetworkHealthProvider::LookupNetwork(
    const network_mojom::NetworkStatePropertiesPtr& network,
    bool must_match_existing_guid) {
  for (auto& pair : networks_) {
    NetworkObserverInfo& info = pair.second;
    if (IsMatchingNetwork(network, info, must_match_existing_guid)) {
      return &info;
    }
  }

  return nullptr;
}

bool NetworkHealthProvider::UpdateActiveGuid() {
  std::string new_active_guid;
  for (auto& pair : networks_) {
    if (new_active_guid.empty()) {
      if (pair.second.network->state == mojom::NetworkState::kOnline ||
          pair.second.network->state == mojom::NetworkState::kConnected) {
        new_active_guid = pair.first;
      }
    } else {
      auto iter = networks_.find(new_active_guid);
      if (iter == networks_.end()) {
        continue;
      }

      // If the current network is online and ethernet and the existing one was
      // only wifi then this is a better match.
      // TODO(michaelcheco): Add a mechanism for ranking/ordering all networks
      // based on connection state and type, and use that instead.
      const bool is_better_match =
          (pair.second.network->state == mojom::NetworkState::kOnline) &&
          (pair.second.network->type == mojom::NetworkType::kEthernet) &&
          (iter->second.network->type == mojom::NetworkType::kWiFi);

      if (is_better_match) {
        new_active_guid = pair.first;
      }
    }
  }

  if (active_guid_ != new_active_guid) {
    active_guid_ = std::move(new_active_guid);
    return true;
  }

  return false;
}

std::vector<std::string> NetworkHealthProvider::GetObserverGuids() {
  std::vector<std::string> observer_guids;
  observer_guids.reserve(networks_.size());
  for (auto& pair : networks_) {
    observer_guids.push_back(pair.first);
  }

  return observer_guids;
}

void NetworkHealthProvider::GetManagedPropertiesForNetwork(
    const std::string& network_guid,
    const std::string& observer_guid) {
  remote_cros_network_config_->GetManagedProperties(
      network_guid,
      base::BindOnce(&NetworkHealthProvider::OnManagedPropertiesReceived,
                     base::Unretained(this), observer_guid));
}

void NetworkHealthProvider::OnManagedPropertiesReceived(
    const std::string& observer_guid,
    network_mojom::ManagedPropertiesPtr managed_properties) {
  auto iter = networks_.find(observer_guid);
  if (iter == networks_.end()) {
    LOG(WARNING) << "Ignoring network that no longer exists guid="
                 << observer_guid;
    return;
  }

  UpdateNetwork(managed_properties, &iter->second);

  // In case the new update to the network changed which network is the primary
  // then update the network list observers as well. This can happen if for
  // example, a WiFi network is currently the primary and then an ethernet
  // network becomes connected.
  NotifyNetworkListObservers();
  NotifyNetworkStateObserver(iter->second);
}

void NetworkHealthProvider::BindInterface(
    mojo::PendingReceiver<mojom::NetworkHealthProvider> pending_receiver) {
  DCHECK(features::IsNetworkingInDiagnosticsAppEnabled());
  receiver_.Bind(std::move(pending_receiver));
}

void NetworkHealthProvider::NotifyNetworkListObservers() {
  UpdateActiveGuid();
  std::vector<std::string> observer_guids = GetObserverGuids();
  for (auto& observer : network_list_observers_) {
    observer->OnNetworkListChanged(mojo::Clone(observer_guids), active_guid_);
  }
}

void NetworkHealthProvider::NotifyNetworkStateObserver(
    const NetworkObserverInfo& network_info) {
  if (!network_info.observer) {
    return;
  }

  network_info.observer->OnNetworkStateChanged(
      mojo::Clone(network_info.network));
}

void NetworkHealthProvider::GetActiveNetworkState() {
  remote_cros_network_config_->GetNetworkStateList(
      network_mojom::NetworkFilter::New(network_mojom::FilterType::kActive,
                                        network_mojom::NetworkType::kAll,
                                        network_mojom::kNoLimit),
      base::BindOnce(&NetworkHealthProvider::OnActiveNetworkStateListReceived,
                     base::Unretained(this)));
}

void NetworkHealthProvider::GetDeviceState() {
  remote_cros_network_config_->GetDeviceStateList(
      base::BindOnce(&NetworkHealthProvider::OnDeviceStateListReceived,
                     base::Unretained(this)));
}

bool NetworkHealthProvider::IsLoggingEnabled() const {
  return networking_log_ptr_ != nullptr;
}

}  // namespace diagnostics
}  // namespace ash
