// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/connectivity/network_health_provider.h"

#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/system/diagnostics/diagnostics_log_controller.h"
#include "ash/system/diagnostics/networking_log.h"
#include "ash/webui/diagnostics_ui/backend/common/histogram_util.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/uuid.h"
#include "chromeos/ash/services/network_config/in_process_instance.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"
#include "mojo/public/cpp/bindings/enum_traits.h"

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

bool IsConnectedOrOnline(mojom::NetworkState state) {
  switch (state) {
    case mojom::NetworkState::kOnline:
    case mojom::NetworkState::kConnected:
    case mojom::NetworkState::kPortal:
      return true;
    case mojom::NetworkState::kNotConnected:
    case mojom::NetworkState::kConnecting:
    case mojom::NetworkState::kDisabled:
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

bool DeviceIsDisabledOrDisabling(network_mojom::DeviceStateType device_state) {
  return device_state == network_mojom::DeviceStateType::kDisabled ||
         device_state == network_mojom::DeviceStateType::kDisabling;
}

constexpr mojom::NetworkState CombineNetworkStates(
    network_mojom::ConnectionStateType connection_state,
    network_mojom::DeviceStateType device_state) {
  if (DeviceIsDisabledOrDisabling(device_state)) {
    return mojom::NetworkState::kDisabled;
  }

  return ConnectionStateToNetworkState(connection_state);
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

mojom::SecurityType ConvertSecurityType(network_mojom::SecurityType type) {
  return mojo::EnumTraits<mojom::SecurityType,
                          network_mojom::SecurityType>::ToMojom(type);
}

mojom::AuthenticationType ConvertAuthenticationType(
    network_mojom::AuthenticationType type) {
  return mojo::EnumTraits<mojom::AuthenticationType,
                          network_mojom::AuthenticationType>::ToMojom(type);
}

mojom::IPConfigPropertiesPtr CreateIPConfigProperties(
    const network_mojom::IPConfigPropertiesPtr& ip_config_props) {
  mojom::IPConfigPropertiesPtr ip_config = mojom::IPConfigProperties::New();
  ip_config->ip_address = ip_config_props->ip_address;
  ip_config->gateway = ip_config_props->gateway;
  ip_config->name_servers = ip_config_props->name_servers;
  // Default to 0 means an unset value.
  ip_config->routing_prefix = 0;
  auto routing_prefix = ip_config_props->routing_prefix;
  bool isIPv4 = ip_config_props->type == network_mojom::IPConfigType::kIPv4;
  if (isnan(routing_prefix)) {
    LOG(ERROR) << "routing_prefix is not a number.";
    EmitNetworkDataError(metrics::DataError::kNotANumber);
  } else if (isIPv4 && (routing_prefix < 0 || routing_prefix > 32)) {
    LOG(ERROR) << "IPv4 routing_prefix should be larger/equal to zero and "
                  "smaller/equal to 32. It is now: "
               << routing_prefix;
    EmitNetworkDataError(metrics::DataError::kExpectationNotMet);
  } else if (!isIPv4 && (routing_prefix < 0 || routing_prefix > 128)) {
    // TODO(wenyu): Add unit test to cover this scenario.
    LOG(ERROR) << "IPv6 routing_prefix should be larger/equal to zero and "
                  "smaller/equal to 128. It is now: "
               << routing_prefix;
    EmitNetworkDataError(metrics::DataError::kExpectationNotMet);
  } else {
    ip_config->routing_prefix = routing_prefix;
  }
  return ip_config;
}

mojom::WiFiStatePropertiesPtr CreateWiFiStateProperties(
    const network_mojom::NetworkTypeStateProperties& network_type_props) {
  auto wifi_props = mojom::WiFiStateProperties::New();
  wifi_props->signal_strength = network_type_props.get_wifi()->signal_strength;
  wifi_props->frequency = network_type_props.get_wifi()->frequency;
  wifi_props->ssid = network_type_props.get_wifi()->ssid;
  wifi_props->bssid = base::ToUpperASCII(network_type_props.get_wifi()->bssid);
  wifi_props->security =
      ConvertSecurityType(network_type_props.get_wifi()->security);
  return wifi_props;
}

mojom::EthernetStatePropertiesPtr CreateEthernetStateProperties(
    const network_mojom::NetworkTypeStateProperties& network_type_props) {
  auto ethernet_props = mojom::EthernetStateProperties::New();
  ethernet_props->authentication = ConvertAuthenticationType(
      network_type_props.get_ethernet()->authentication);
  return ethernet_props;
}

mojom::CellularStatePropertiesPtr CreateCellularStateProperties(
    const network_mojom::NetworkTypeStateProperties& network_type_props) {
  auto cellular_props = mojom::CellularStateProperties::New();
  cellular_props->iccid = network_type_props.get_cellular()->iccid;
  cellular_props->eid = network_type_props.get_cellular()->eid;
  cellular_props->network_technology =
      network_type_props.get_cellular()->network_technology;
  cellular_props->roaming = network_type_props.get_cellular()->roaming;
  cellular_props->signal_strength =
      network_type_props.get_cellular()->signal_strength;
  return cellular_props;
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

mojom::RoamingState GetRoamingState(std::optional<std::string>& roaming_state) {
  if (!roaming_state.has_value()) {
    return mojom::RoamingState::kNone;
  }

  std::string state = roaming_state.value();
  // Possible values are 'Home' and 'Roaming'.
  DCHECK(state == "Home" || state == "Roaming");
  return state == "Home" ? mojom::RoamingState::kHome
                         : mojom::RoamingState::kRoaming;
}

constexpr mojom::LockType GetLockType(const std::string& lock_type) {
  // Possible values are 'sim-pin', 'sim-puk', 'network-pin' or empty.
  if (lock_type.empty()) {
    return mojom::LockType::kNone;
  }

  DCHECK(lock_type == "sim-pin" || lock_type == "sim-puk" ||
         lock_type == "network-pin");

  if (lock_type == "sim-pin") {
    return mojom::LockType::kSimPin;
  }
  if (lock_type == "sim-puk") {
    return mojom::LockType::kSimPuk;
  }
  if (lock_type == "network-pin") {
    return mojom::LockType::kNetworkPin;
  }
  return mojom::LockType::kNone;
}

void UpdateNetwork(
    const network_mojom::NetworkTypeStateProperties& network_type_props,
    mojom::Network* network) {
  switch (network->type) {
    case mojom::NetworkType::kWiFi:
      network->type_properties = mojom::NetworkTypeProperties::NewWifi(
          CreateWiFiStateProperties(network_type_props));
      break;
    case mojom::NetworkType::kEthernet:
      network->type_properties = mojom::NetworkTypeProperties::NewEthernet(
          CreateEthernetStateProperties(network_type_props));
      break;
    case mojom::NetworkType::kCellular: {
      auto cellular_props = CreateCellularStateProperties(network_type_props);
      // If we have existing Cellular type properties, i.e., Sim Lock Status
      // was present for this network, combine the newly created Cellular
      // properties with the existing ones.
      if (network->type_properties) {
        cellular_props->lock_type =
            network->type_properties->get_cellular()->lock_type;
        cellular_props->sim_locked =
            network->type_properties->get_cellular()->sim_locked;
        cellular_props->roaming_state =
            network->type_properties->get_cellular()->roaming_state;
      }
      network->type_properties =
          mojom::NetworkTypeProperties::NewCellular(std::move(cellular_props));
      break;
    }
    case mojom::NetworkType::kUnsupported:
      NOTREACHED();
  }
}

void UpdateNetwork(
    const network_mojom::NetworkStatePropertiesPtr& network_state,
    NetworkObserverInfo* network_info) {
  mojom::Network* network = network_info->network.get();
  network->name = network_state->name;
  network->state = CombineNetworkStates(network_state->connection_state,
                                        network_info->device_state);

  // Network type must be populated before calling UpdateNetwork.
  network->type = ConvertNetworkType(network_state->type);

  if (IsConnectedOrConnecting(network_state->connection_state)) {
    network_info->network_guid = network_state->guid;
    UpdateNetwork(*network_state->type_state, network);
    return;
  }

  ClearDisconnectedNetwork(network_info);
}

void CreateEmptyCellularPropertiesForNetwork(mojom::Network* network) {
  auto cellular_props = mojom::CellularStateProperties::New();
  network->type_properties =
      mojom::NetworkTypeProperties::NewCellular(std::move(cellular_props));
}

void UpdateNetwork(const network_mojom::DeviceStatePropertiesPtr& device,
                   NetworkObserverInfo* network_info) {
  mojom::Network* network = network_info->network.get();
  DCHECK(!network->observer_guid.empty());

  network->type = ConvertNetworkType(device->type);
  network->mac_address = device->mac_address;
  network_info->device_state = device->device_state;
  if (network->type == mojom::NetworkType::kCellular &&
      device->sim_lock_status) {
    // Create partially populated CellularStateProperties struct.
    // Remaining properties will be added in CreateCellularStateProperties.
    CreateEmptyCellularPropertiesForNetwork(network);
    DCHECK(network->type_properties->is_cellular());
    network->type_properties->get_cellular()->lock_type =
        GetLockType(device->sim_lock_status->lock_type);
    network->type_properties->get_cellular()->sim_locked =
        device->sim_lock_status->lock_enabled;
  }

  if (DeviceIsDisabledOrDisabling(device->device_state)) {
    network->state = mojom::NetworkState::kDisabled;
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

  if (network->type == mojom::NetworkType::kCellular && managed_properties) {
    auto roaming_state = GetRoamingState(
        managed_properties->type_properties->get_cellular()->roaming_state);
    if (!network->type_properties) {
      CreateEmptyCellularPropertiesForNetwork(network);
    }
    DCHECK(network->type_properties->is_cellular());
    network->type_properties->get_cellular()->roaming_state = roaming_state;
  }
}

// Calculate a score for a network based on its type and state.
// Network state takes precedence over network type. In the case
// of a tie (Ethernet Connected & WiFi Connected for ex), the network
// type considered to have the higher priority will take precedence.
int GetScoreForNetwork(const mojom::NetworkPtr& network) {
  static constexpr auto kNetworkStatePriorityMap =
      base::MakeFixedFlatMap<mojom::NetworkState, int>(
          {{mojom::NetworkState::kOnline, 300},
           {mojom::NetworkState::kPortal, 200},
           {mojom::NetworkState::kConnected, 100}});

  static constexpr auto kNetworkTypePriorityMap =
      base::MakeFixedFlatMap<mojom::NetworkType, int>(
          {{mojom::NetworkType::kEthernet, 3},
           {mojom::NetworkType::kWiFi, 2},
           {mojom::NetworkType::kCellular, 1}});

  int state_priority = 0;
  if (base::Contains(kNetworkStatePriorityMap, network->state)) {
    state_priority += kNetworkStatePriorityMap.at(network->state);
  }

  DCHECK(base::Contains(kNetworkTypePriorityMap, network->type));
  return kNetworkTypePriorityMap.at(network->type) + state_priority;
}

bool IsLoggingEnabled() {
  return diagnostics::DiagnosticsLogController::IsInitialized();
}

}  // namespace

NetworkObserverInfo::NetworkObserverInfo() = default;
NetworkObserverInfo::NetworkObserverInfo(NetworkObserverInfo&&) = default;
NetworkObserverInfo& NetworkObserverInfo::operator=(NetworkObserverInfo&&) =
    default;
NetworkObserverInfo::~NetworkObserverInfo() = default;

NetworkHealthProvider::NetworkHealthProvider() {
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
  GetManagedPropertiesForNetwork(network->guid,
                                 network_info->network->observer_guid);
}

void NetworkHealthProvider::OnDeviceStateListReceived(
    std::vector<network_mojom::DeviceStatePropertiesPtr> devices) {
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
      std::string observer_guid = AddNewNetwork(device);
      networks_seen.insert(std::move(observer_guid));
    }
  }

  // Remove any entry in |networks_| that doesn't match a device.
  for (auto it = networks_.begin(); it != networks_.end();) {
    const std::string& observer_guid = it->first;
    if (!base::Contains(networks_seen, observer_guid)) {
      it = networks_.erase(it);
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
    const network_mojom::DeviceStatePropertiesPtr& device) {
  std::string observer_guid =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  auto network = mojom::Network::New();
  network->observer_guid = observer_guid;

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

std::vector<std::string>
NetworkHealthProvider::GetObserverGuidsAndUpdateActiveGuid() {
  std::vector<std::string> observer_guids;
  observer_guids.reserve(networks_.size());
  for (auto& pair : networks_) {
    observer_guids.push_back(pair.first);
  }

  // Sort list of observer guids in descending order based on score.
  sort(observer_guids.begin(), observer_guids.end(),
       [&](const std::string& lhs, const std::string& rhs) -> bool {
         mojom::NetworkPtr& network1 = networks_.at(lhs).network;
         mojom::NetworkPtr& network2 = networks_.at(rhs).network;
         return GetScoreForNetwork(network1) > GetScoreForNetwork(network2);
       });

  // Update |active_guid_| if the observer guid with the highest score
  // corresponds to a network interface that is either online or connected.
  std::string new_active_guid;
  if (observer_guids.size() > 0) {
    const std::string& guid = observer_guids[0];
    if (IsConnectedOrOnline(GetNetworkStateForGuid(guid))) {
      new_active_guid = guid;
    }
  }

  active_guid_ = std::move(new_active_guid);
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
  receiver_.reset();
  receiver_.Bind(std::move(pending_receiver));
}

void NetworkHealthProvider::NotifyNetworkListObservers() {
  std::vector<std::string> observer_guids =
      GetObserverGuidsAndUpdateActiveGuid();
  for (auto& observer : network_list_observers_) {
    observer->OnNetworkListChanged(mojo::Clone(observer_guids), active_guid_);
  }

  if (IsLoggingEnabled() && !active_guid_.empty()) {
    DiagnosticsLogController::Get()->GetNetworkingLog().UpdateNetworkList(
        observer_guids, active_guid_);
  }
}

void NetworkHealthProvider::NotifyNetworkStateObserver(
    const NetworkObserverInfo& network_info) {
  if (!network_info.observer) {
    return;
  }

  network_info.observer->OnNetworkStateChanged(
      mojo::Clone(network_info.network));

  if (IsLoggingEnabled()) {
    DiagnosticsLogController::Get()->GetNetworkingLog().UpdateNetworkState(
        network_info.network.Clone());
  }
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

mojom::NetworkState NetworkHealthProvider::GetNetworkStateForGuid(
    const std::string& guid) {
  auto it = networks_.find(guid);
  DCHECK(it != networks_.end());
  return it->second.network->state;
}

}  // namespace diagnostics
}  // namespace ash
