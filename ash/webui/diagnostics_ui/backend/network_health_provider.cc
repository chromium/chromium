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

bool IsNetworkOnline(network_mojom::ConnectionStateType connection_state) {
  return connection_state == network_mojom::ConnectionStateType::kOnline;
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
      NOTREACHED();
      return mojom::NetworkType::kUnsupported;
  }
}

mojom::IPConfigPropertiesPtr PopulateIPConfigProperties(
    network_mojom::IPConfigProperties* ip_config_props) {
  mojom::IPConfigPropertiesPtr ip_config = mojom::IPConfigProperties::New();
  ip_config->ip_address = ip_config_props->ip_address;
  ip_config->routing_prefix = ip_config_props->routing_prefix;
  ip_config->gateway = ip_config_props->gateway;
  ip_config->name_servers = ip_config_props->name_servers;
  return ip_config;
}

mojom::WiFiStatePropertiesPtr PopulateWiFiStateProperties(
    network_mojom::NetworkTypeStateProperties* network_type_props) {
  auto wifi_props = mojom::WiFiStateProperties::New();
  wifi_props->signal_strength = network_type_props->get_wifi()->signal_strength;
  wifi_props->frequency = network_type_props->get_wifi()->frequency;
  wifi_props->ssid = network_type_props->get_wifi()->ssid;
  wifi_props->bssid = network_type_props->get_wifi()->bssid;
  return wifi_props;
}

// TODO(michaelcheco): Add Ethernet properties.
mojom::EthernetStatePropertiesPtr PopulateEthernetStateProperties(
    network_mojom::NetworkTypeStateProperties* network_type_props) {
  return mojom::EthernetStateProperties::New();
}

// TODO(michaelcheco): Add Cellular properties.
mojom::CellularStatePropertiesPtr PopulateCellularStateProperties(
    network_mojom::NetworkTypeStateProperties* network_type_props) {
  return mojom::CellularStateProperties::New();
}

// Uses the network type to determine which network properties to
// add the mojom::Network struct.
mojom::NetworkTypePropertiesPtr PopulateNetworkTypeProperties(
    network_mojom::NetworkTypeStateProperties* network_type_props,
    mojom::NetworkType type) {
  auto type_properties = mojom::NetworkTypeProperties::New();
  switch (type) {
    case mojom::NetworkType::kWiFi:
      type_properties->set_wifi(
          PopulateWiFiStateProperties(network_type_props));
      break;
    case mojom::NetworkType::kEthernet:
      type_properties->set_ethernet(
          PopulateEthernetStateProperties(network_type_props));
      break;
    case mojom::NetworkType::kCellular:
      type_properties->set_cellular(
          PopulateCellularStateProperties(network_type_props));
      break;
    case mojom::NetworkType::kUnsupported:
      NOTREACHED();
      break;
  }
  return type_properties;
}

mojom::NetworkPtr CreateNetwork(const NetworkProperties& network_props,
                                network_mojom::DeviceStateProperties* device) {
  auto network = mojom::Network::New();
  network->guid = network_props.network_state->guid;
  network->name = network_props.network_state->name;
  network->state = ConnectionStateToNetworkState(
      network_props.network_state->connection_state);
  network->type = ConvertNetworkType(network_props.network_state->type);
  network->type_properties = PopulateNetworkTypeProperties(
      network_props.network_state->type_state.get(), network->type);
  const bool has_ip_config = network_props.managed_properties &&
                             network_props.managed_properties->saved_ip_config;
  if (has_ip_config) {
    network->ip_config = PopulateIPConfigProperties(
        network_props.managed_properties->saved_ip_config.get());
  }

  if (device) {
    network->mac_address = device->mac_address;
  }

  return network;
}

}  // namespace

NetworkProperties::NetworkProperties(
    network_mojom::NetworkStatePropertiesPtr network_state)
    : network_state(std::move(network_state)) {}

NetworkProperties::~NetworkProperties() = default;

NetworkHealthProvider::NetworkHealthProvider() {
  network_config::BindToInProcessInstance(
      remote_cros_network_config_.BindNewPipeAndPassReceiver());
  remote_cros_network_config_->AddObserver(
      cros_network_config_observer_receiver_.BindNewPipeAndPassRemote());

  // Fetch initial network and device state.
  GetNetworkState();
  GetDeviceState();
}

NetworkHealthProvider::~NetworkHealthProvider() = default;

NetworkProperties& NetworkHealthProvider::GetNetworkProperties(
    const std::string& guid) {
  DCHECK(base::Contains(network_properties_map_, guid));
  return network_properties_map_.at(guid);
}

void NetworkHealthProvider::GetNetworkState() {
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

void NetworkHealthProvider::OnNetworkStateListChanged() {}

void NetworkHealthProvider::OnDeviceStateListChanged() {
  GetDeviceState();
}

void NetworkHealthProvider::OnActiveNetworksChanged(
    std::vector<network_mojom::NetworkStatePropertiesPtr> active_networks) {
  OnActiveNetworkStateListReceived(std::move(active_networks));
}

void NetworkHealthProvider::OnNetworkStateChanged(
    network_mojom::NetworkStatePropertiesPtr network_state) {
  if (base::Contains(network_properties_map_, network_state->guid)) {
    NotifyNetworkStateObserver(GetNetworkProperties(network_state->guid));
  }
}

void NetworkHealthProvider::OnVpnProvidersChanged() {}
void NetworkHealthProvider::OnNetworkCertificatesChanged() {}

void NetworkHealthProvider::OnActiveNetworkStateListReceived(
    std::vector<network_mojom::NetworkStatePropertiesPtr> networks) {
  network_properties_map_.clear();
  active_guid_.clear();
  for (auto& network : networks) {
    if (IsSupportedNetworkType(network->type)) {
      const std::string guid = mojo::Clone(network->guid);
      network_mojom::ConnectionStateType connection_state =
          mojo::Clone(network->connection_state);
      network_properties_map_.emplace(guid, std::move(network));
      if (IsNetworkOnline(connection_state)) {
        active_guid_ = guid;
      }
      // This method depends on the |network_properties_map_| being populated
      // before being called.
      GetManagedPropertiesForNetwork(guid);
    }
  }
  NotifyNetworkListObservers();
}

void NetworkHealthProvider::OnDeviceStateListReceived(
    std::vector<network_mojom::DeviceStatePropertiesPtr> devices) {
  device_type_map_.clear();
  for (auto& device : devices) {
    if (IsSupportedNetworkType(device->type)) {
      device_type_map_.emplace(device->type, std::move(device));
    }
  }
}

std::vector<std::string> NetworkHealthProvider::GetNetworkGuidList() {
  std::vector<std::string> network_guids;
  network_guids.reserve(network_properties_map_.size());
  for (const auto& entry : network_properties_map_) {
    network_guids.push_back(entry.first);
  }
  return network_guids;
}

const DeviceMap& NetworkHealthProvider::GetDeviceTypeMapForTesting() {
  return device_type_map_;
}

const NetworkPropertiesMap&
NetworkHealthProvider::GetNetworkPropertiesMapForTesting() {
  return network_properties_map_;
}

void NetworkHealthProvider::GetManagedPropertiesForNetwork(
    const std::string& guid) {
  remote_cros_network_config_->GetManagedProperties(
      guid, base::BindOnce(&NetworkHealthProvider::OnManagedPropertiesReceived,
                           base::Unretained(this), guid));
}

void NetworkHealthProvider::OnManagedPropertiesReceived(
    const std::string& guid,
    network_mojom::ManagedPropertiesPtr managed_properties) {
  if (!managed_properties) {
    DVLOG(1) << "No managed properties found for guid: " << guid;
    return;
  }

  // Add managed properties to corresponding NetworkProperties struct.
  NetworkProperties& network_properties = GetNetworkProperties(guid);
  network_properties.managed_properties = std::move(managed_properties);
}

network_mojom::DeviceStateProperties* NetworkHealthProvider::GetMatchingDevice(
    network_mojom::NetworkType type) {
  auto device_iter = device_type_map_.find(type);
  if (device_iter != device_type_map_.end()) {
    return device_iter->second.get();
  }
  return nullptr;
}

void NetworkHealthProvider::ObserveNetworkList(
    mojo::PendingRemote<mojom::NetworkListObserver> observer) {
  network_list_observers_.Add(std::move(observer));
  NotifyNetworkListObservers();
}

void NetworkHealthProvider::ObserveNetwork(
    mojo::PendingRemote<mojom::NetworkStateObserver> observer,
    const std::string& guid) {
  NetworkProperties& network_properties = GetNetworkProperties(guid);
  network_properties.observer =
      mojo::Remote<mojom::NetworkStateObserver>(std::move(observer));
  NotifyNetworkStateObserver(network_properties);
}

void NetworkHealthProvider::BindInterface(
    mojo::PendingReceiver<mojom::NetworkHealthProvider> pending_receiver) {
  DCHECK(features::IsNetworkingInDiagnosticsAppEnabled());
  receiver_.Bind(std::move(pending_receiver));
}

void NetworkHealthProvider::NotifyNetworkListObservers() {
  auto network_guid_list = GetNetworkGuidList();
  for (auto& observer : network_list_observers_) {
    observer->OnNetworkListChanged(mojo::Clone(network_guid_list),
                                   active_guid_);
  }
}

void NetworkHealthProvider::NotifyNetworkStateObserver(
    const NetworkProperties& network_props) {
  if (!network_props.observer) {
    return;
  }

  mojom::NetworkPtr network = CreateNetwork(
      network_props, GetMatchingDevice(network_props.network_state->type));
  network_props.observer->OnNetworkStateChanged(std::move(network));
}

}  // namespace diagnostics
}  // namespace ash
