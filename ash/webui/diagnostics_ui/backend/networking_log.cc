// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/networking_log.h"

#include <sstream>
#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/i18n/time_formatting.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"

namespace ash {
namespace diagnostics {
namespace {

const char kNewline[] = "\n";

// NetworkingInfo constants:
const char kNetworkingInfoSectionName[] = "--- Network Info ---";
const char kNetworkNameTitle[] = "Name: ";
const char kNetworkTypeTitle[] = "Type: ";
const char kNetworkStateTitle[] = "State: ";
const char kActiveTitle[] = "Active: ";
const char kMacAddressTitle[] = "MAC Address: ";

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

void AddWifiInfoToLog(const mojom::NetworkTypeProperties& type_props,
                      std::stringstream& output) {
  output << kWifiSignalStrengthTitle << type_props.get_wifi()->signal_strength
         << kNewline << kWifiFrequencyTitle << type_props.get_wifi()->frequency
         << kNewline << kWifiSsidTitle << type_props.get_wifi()->ssid
         << kNewline << kWifiBssidTitle << type_props.get_wifi()->bssid
         << kNewline << kSecurityTitle
         << GetSecurityType(type_props.get_wifi()->security) << kNewline;
}

void AddCellularInfoToLog(const mojom::NetworkTypeProperties& type_props,
                          std::stringstream& output) {
  // TODO(michaelcheco): Add Cellular type properties to log.
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
      break;
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
      return "";
  }
}

}  // namespace

NetworkingLog::NetworkingLog() = default;
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

void NetworkingLog::UpdateNetworkList(
    const std::vector<std::string>& observer_guids,
    std::string active_guid) {
  // If a network is no longer valid, remove it from the map.
  for (auto iter = latest_network_states_.begin();
       iter != latest_network_states_.end();) {
    if (!base::Contains(observer_guids, iter->first)) {
      iter = latest_network_states_.erase(iter);
      continue;
    }

    ++iter;
  }

  active_guid_ = active_guid;
}

void NetworkingLog::UpdateNetworkState(mojom::NetworkPtr network) {
  latest_network_states_[network->observer_guid] = std::move(network);
}

}  // namespace diagnostics
}  // namespace ash
