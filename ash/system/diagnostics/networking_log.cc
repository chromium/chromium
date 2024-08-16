// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/diagnostics/networking_log.h"

#include <sstream>
#include <utility>

#include "base/check.h"
#include "base/check_is_test.h"
#include "base/containers/contains.h"
#include "base/i18n/time_formatting.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"

namespace ash {
namespace diagnostics {
namespace {

const char kNewline[] = "\n";

// NetworkingInfo constants:
const char kNetworkingInfoSectionName[] = "--- Network Info ---";
const char kNetworkEventsSectionName[] = "--- Network Events ---";
const char kNetworkNameTitle[] = "Name: ";
const char kNetworkTypeTitle[] = "Type: ";
const char kNetworkStateTitle[] = "State: ";
const char kActiveTitle[] = "Active: ";
const char kMacAddressTitle[] = "MAC Address: ";

// CellularStateProperties constants:
const char kCellularIccidTitle[] = "ICCID: ";
const char kCellularEidTitle[] = "EID: ";
const char kCellularNetworkTechnologyTitle[] = "Technology: ";
const char kCellularRoamingTitle[] = "Roaming: ";
const char kCellularRoamingStateTitle[] = "Roaming State: ";
const char kCellularSignalStrengthTitle[] = "Signal Strength: ";
const char kCellularSimLockedTitle[] = "SIM Locked: ";
const char kCellularLockTypeTitle[] = "SIM Lock Type: ";

// EthernetStateProperties constants:
const char kEthernetAuthenticationTitle[] = "Authentication: ";

// WiFiStateProperties constants:
const char kWifiSignalStrengthTitle[] = "Signal Strength: ";
const char kWifiFrequencyTitle[] = "Frequency: ";
const char kWifiSsidTitle[] = "SSID: ";
const char kWifiBssidTitle[] = "BSSID: ";
const char kSecurityTitle[] = "Security: ";

// IpConfigProperties constants:
const char kNameServersTitle[] = "Name Servers: ";
const char kGatewayTitle[] = "Gateway: ";
const char kIPAddressTitle[] = "IP Address: ";
const char kSubnetMaskTitle[] = "Subnet Mask: ";

// Event log entries
const char kEventLogFilename[] = "network_events.log";
const char kNetworkAddedEventTemplate[] =
    "%s network [%s] started in state %s\n";
const char kNetworkRemovedEventTemplate[] = "%s network [%s] removed\n";
const char kNetworkStateChangedEventTemplate[] =
    "%s network [%s] changed state from %s to %s\n";
const char kJoinedWiFiEventTemplate[] =
    "%s network [%s] joined SSID '%s' on access point [%s]\n";
const char kLeftWiFiEventTemplate[] = "%s network [%s] left SSID '%s'\n";
const char kAccessPointRoamingEventTemplate[] =
    "%s network [%s] on SSID '%s' roamed from access point [%s] to [%s]\n";

std::string GetSubnetMask(int prefix) {
  uint32_t mask = (0xFFFFFFFF << (32 - prefix)) & 0xFFFFFFFF;
  std::vector<uint32_t> pieces = {mask >> 24, (mask >> 16) & 0xFF,
                                  (mask >> 8) & 0xFF, mask & 0xFF};
  std::vector<std::string> vec;
  for (const auto& piece : pieces) {
    vec.push_back(base::NumberToString(piece));
  }

  return base::JoinString(vec, ".");
}

std::string GetSecurityType(mojom::SecurityType type) {
  switch (type) {
    case mojom::SecurityType::kNone:
      return "None";
    case mojom::SecurityType::kWep8021x:
    case mojom::SecurityType::kWpaEap:
      return "EAP";
    case mojom::SecurityType::kWepPsk:
      return "WEP";
    case mojom::SecurityType::kWpaPsk:
      return "PSK (WPA or RSN)";
  }
}

std::string GetSsid(const mojom::NetworkPtr& network) {
  DCHECK(network->type == mojom::NetworkType::kWiFi);
  return network->type_properties ? network->type_properties->get_wifi()->ssid
                                  : "";
}

std::string GetBssid(const mojom::NetworkPtr& network) {
  DCHECK(network->type == mojom::NetworkType::kWiFi);
  return network->type_properties ? network->type_properties->get_wifi()->bssid
                                  : "";
}

bool HasJoinedWiFiNetwork(const mojom::NetworkPtr& old_state,
                          const mojom::NetworkPtr& new_state) {
  const std::string old_ssid = GetSsid(old_state);
  return old_ssid.empty() && (old_ssid != GetSsid(new_state));
}

bool HasLeftWiFiNetwork(const mojom::NetworkPtr& old_state,
                        const mojom::NetworkPtr& new_state) {
  const std::string new_ssid = GetSsid(new_state);
  return new_ssid.empty() && (new_ssid != GetSsid(old_state));
}

bool HasRoamedAccessPoint(const mojom::NetworkPtr& old_state,
                          const mojom::NetworkPtr& new_state) {
  const std::string new_ssid = GetSsid(new_state);
  return !new_ssid.empty() && (new_ssid == GetSsid(old_state)) &&
         (GetBssid(old_state) != GetBssid(new_state));
}

void AddWifiInfoToLog(const mojom::NetworkTypeProperties& type_props,
                      std::stringstream& output) {
  output << kWifiSignalStrengthTitle << type_props.get_wifi()->signal_strength
         << kNewline << kWifiFrequencyTitle << type_props.get_wifi()->frequency
         << kNewline << kWifiSsidTitle << type_props.get_wifi()->ssid
         << kNewline << kWifiBssidTitle << type_props.get_wifi()->bssid
         << kNewline << kSecurityTitle
         << GetSecurityType(type_props.get_wifi()->security) << kNewline;
}

std::string GetBoolAsString(bool value) {
  return value ? "True" : "False";
}

std::string GetCellularRoamingState(mojom::RoamingState state) {
  switch (state) {
    case mojom::RoamingState::kNone:
      return "None";
    case mojom::RoamingState::kHome:
      return "Home";
    case mojom::RoamingState::kRoaming:
      return "Roaming";
  }
}

std::string GetCellularLockType(mojom::LockType lock_type) {
  switch (lock_type) {
    case mojom::LockType::kNone:
      return "None";
    case mojom::LockType::kSimPin:
      return "sim-pin";
    case mojom::LockType::kSimPuk:
      return "sim-puk";
    case mojom::LockType::kNetworkPin:
      return "network-pin";
  }
}

void AddCellularInfoToLog(const mojom::NetworkTypeProperties& type_props,
                          std::stringstream& output) {
  output << kCellularIccidTitle << type_props.get_cellular()->iccid << kNewline
         << kCellularEidTitle << type_props.get_cellular()->eid << kNewline
         << kCellularNetworkTechnologyTitle
         << type_props.get_cellular()->network_technology << kNewline
         << kCellularRoamingTitle
         << GetBoolAsString(type_props.get_cellular()->roaming) << kNewline
         << kCellularRoamingStateTitle
         << GetCellularRoamingState(type_props.get_cellular()->roaming_state)
         << kNewline << kCellularSignalStrengthTitle
         << type_props.get_cellular()->signal_strength << kNewline
         << kCellularSimLockedTitle
         << GetBoolAsString(type_props.get_cellular()->sim_locked) << kNewline
         << kCellularLockTypeTitle
         << GetCellularLockType(type_props.get_cellular()->lock_type)
         << kNewline;
}

std::string GetEthernetAuthenticationType(mojom::AuthenticationType type) {
  switch (type) {
    case mojom::AuthenticationType::kNone:
      return "None";
    case mojom::AuthenticationType::k8021x:
      return "EAP";
  }
}

void AddEthernetInfoToLog(const mojom::NetworkTypeProperties& type_props,
                          std::stringstream& output) {
  output << kEthernetAuthenticationTitle
         << GetEthernetAuthenticationType(
                type_props.get_ethernet()->authentication)
         << kNewline;
}

void AddTypePropertiesToLog(const mojom::NetworkTypeProperties& type_props,
                            mojom::NetworkType type,
                            std::stringstream& output) {
  switch (type) {
    case mojom::NetworkType::kWiFi:
      AddWifiInfoToLog(type_props, output);
      break;
    case mojom::NetworkType::kCellular:
      AddCellularInfoToLog(type_props, output);
      break;
    case mojom::NetworkType::kEthernet:
      AddEthernetInfoToLog(type_props, output);
      break;
    case mojom::NetworkType::kUnsupported:
      NOTREACHED();
  }
}

void AddIPConfigPropertiesToLog(const mojom::IPConfigProperties& ip_config,
                                std::stringstream& output) {
  output << kGatewayTitle << ip_config.gateway.value_or("") << kNewline;

  output << kIPAddressTitle << ip_config.ip_address.value_or("") << kNewline;

  auto name_servers = base::JoinString(
      ip_config.name_servers.value_or(std::vector<std::string>()), ", ");

  output << kNameServersTitle << name_servers << kNewline;

  // A routing prefix can not be 0, 0 indicates an unset value.
  auto subnet_mask = ip_config.routing_prefix != 0
                         ? GetSubnetMask(ip_config.routing_prefix)
                         : "";
  output << kSubnetMaskTitle << subnet_mask << kNewline;
}

std::string GetNetworkStateString(mojom::NetworkState state) {
  switch (state) {
    case mojom::NetworkState::kOnline:
      return "Online";
    case mojom::NetworkState::kConnected:
      return "Connected";
    case mojom::NetworkState::kConnecting:
      return "Connecting";
    case mojom::NetworkState::kNotConnected:
      return "Not Connected";
    case mojom::NetworkState::kDisabled:
      return "Disabled";
    case mojom::NetworkState::kPortal:
      return "Portal";
  }
}

std::string GetNetworkType(mojom::NetworkType type) {
  switch (type) {
    case mojom::NetworkType::kWiFi:
      return "WiFi";
    case mojom::NetworkType::kCellular:
      return "Cellular";
    case mojom::NetworkType::kEthernet:
      return "Ethernet";
    case mojom::NetworkType::kUnsupported:
      NOTREACHED();
  }
}

}  // namespace

NetworkingLog::NetworkingLog(const base::FilePath& log_base_path)
    : event_log_(log_base_path.Append(kEventLogFilename)) {}

NetworkingLog::~NetworkingLog() = default;

std::string NetworkingLog::GetNetworkInfo() const {
  std::stringstream output;

  output << kNetworkingInfoSectionName << kNewline;
  for (const auto& pair : latest_network_states_) {
    const mojom::NetworkPtr& network = pair.second;

    output << kNewline << kNetworkNameTitle << network->name << kNewline
           << kNetworkTypeTitle << GetNetworkType(network->type) << kNewline
           << kNetworkStateTitle << GetNetworkStateString(network->state)
           << kNewline << kActiveTitle
           << (network->observer_guid == active_guid_ ? "True" : "False")
           << kNewline << kMacAddressTitle << network->mac_address.value_or("")
           << kNewline;

    if (network->type_properties) {
      AddTypePropertiesToLog(*network->type_properties, network->type, output);
    }

    if (network->ip_config) {
      AddIPConfigPropertiesToLog(*network->ip_config, output);
    }
  }

  return output.str();
}

std::string NetworkingLog::GetNetworkEvents() const {
  return std::string(kNetworkEventsSectionName) + kNewline + kNewline +
         event_log_.GetContents();
}

void NetworkingLog::UpdateNetworkList(
    const std::vector<std::string>& observer_guids,
    std::string active_guid) {
  // If a network is no longer valid, remove it from the map.
  for (auto iter = latest_network_states_.begin();
       iter != latest_network_states_.end();) {
    if (!base::Contains(observer_guids, iter->first)) {
      LogNetworkRemoved(iter->second);
      iter = latest_network_states_.erase(iter);
      continue;
    }

    ++iter;
  }

  active_guid_ = active_guid;
  ++update_network_list_call_count_for_testing_;
}

void NetworkingLog::UpdateNetworkState(mojom::NetworkPtr network) {
  if (network.is_null()) {
    LOG(ERROR) << "Network to log update is null";
    return;
  }

  if (!base::Contains(latest_network_states_, network->observer_guid)) {
    LogNetworkAdded(network);
    latest_network_states_.emplace(network->observer_guid, std::move(network));
    return;
  }

  LogNetworkChanges(network);
  latest_network_states_[network->observer_guid] = std::move(network);
}

void NetworkingLog::LogEvent(const std::string& event_string) {
  const std::string datetime =
      base::UTF16ToUTF8(base::TimeFormatShortDateAndTime(base::Time::Now()));
  event_log_.Append(datetime + " - " + event_string);
}

void NetworkingLog::LogNetworkAdded(const mojom::NetworkPtr& network) {
  const std::string line = base::StringPrintf(
      kNetworkAddedEventTemplate, GetNetworkType(network->type).c_str(),
      network->mac_address.value_or("").c_str(),
      GetNetworkStateString(network->state).c_str());
  LogEvent(line);
}

void NetworkingLog::LogNetworkRemoved(const mojom::NetworkPtr& network) {
  if (network.is_null()) {
    LOG(ERROR) << "Network to log removal is null";
    return;
  }

  const std::string line = base::StringPrintf(
      kNetworkRemovedEventTemplate, GetNetworkType(network->type).c_str(),
      network->mac_address.value_or("").c_str());
  LogEvent(line);
}

void NetworkingLog::LogNetworkChanges(const mojom::NetworkPtr& new_state) {
  DCHECK(base::Contains(latest_network_states_, new_state->observer_guid));
  const mojom::NetworkPtr& old_state =
      latest_network_states_.at(new_state->observer_guid);

  if (new_state->type == mojom::NetworkType::kWiFi) {
    if (HasJoinedWiFiNetwork(old_state, new_state)) {
      LogJoinedWiFiNetwork(new_state);
    } else if (HasLeftWiFiNetwork(old_state, new_state)) {
      LogLeftWiFiNetwork(new_state, GetSsid(old_state));
    } else if (HasRoamedAccessPoint(old_state, new_state)) {
      LogWiFiRoamedAccessPoint(new_state, GetBssid(old_state));
    }
  }

  if (old_state->state != new_state->state) {
    LogNetworkStateChanged(old_state, new_state);
  }
}

void NetworkingLog::LogNetworkStateChanged(const mojom::NetworkPtr& old_state,
                                           const mojom::NetworkPtr& new_state) {
  const std::string line =
      base::StringPrintf(kNetworkStateChangedEventTemplate,
                         GetNetworkType(new_state->type).c_str(),
                         new_state->mac_address.value_or("").c_str(),
                         GetNetworkStateString(old_state->state).c_str(),
                         GetNetworkStateString(new_state->state).c_str());
  LogEvent(line);
}

void NetworkingLog::LogJoinedWiFiNetwork(const mojom::NetworkPtr& network) {
  const std::string line = base::StringPrintf(
      kJoinedWiFiEventTemplate, GetNetworkType(network->type).c_str(),
      network->mac_address.value_or("").c_str(), GetSsid(network).c_str(),
      GetBssid(network).c_str());
  LogEvent(line);
}

void NetworkingLog::LogLeftWiFiNetwork(const mojom::NetworkPtr& network,
                                       const std::string& old_ssid) {
  const std::string line = base::StringPrintf(
      kLeftWiFiEventTemplate, GetNetworkType(network->type).c_str(),
      network->mac_address.value_or("").c_str(), old_ssid.c_str());
  LogEvent(line);
}

void NetworkingLog::LogWiFiRoamedAccessPoint(const mojom::NetworkPtr& network,
                                             const std::string& old_bssid) {
  const std::string line = base::StringPrintf(
      kAccessPointRoamingEventTemplate, GetNetworkType(network->type).c_str(),
      network->mac_address.value_or("").c_str(), GetSsid(network).c_str(),
      old_bssid.c_str(), GetBssid(network).c_str());
  LogEvent(line);
}

size_t NetworkingLog::update_network_list_call_count_for_testing() const {
  CHECK_IS_TEST();
  return update_network_list_call_count_for_testing_;
}

}  // namespace diagnostics
}  // namespace ash
