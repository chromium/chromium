// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/networking_log.h"

#include <sstream>
#include <utility>

#include "base/check.h"
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
const char kNetworkingInfoSectionName[] = "--- Networking Info ---";
const char kNetworkNameTitle[] = "Name: ";
const char kNetworkTypeTitle[] = "Type: ";
const char kNetworkStateTitle[] = "State: ";
const char kMacAddressTitle[] = "MAC Address: ";

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

void AddEthernetInfoToLog(const mojom::NetworkTypeProperties& type_props,
                          std::stringstream& output) {
  // TODO(michaelcheco): Add Ethernet type properties to log.
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
  DCHECK(state == mojom::NetworkState::kOnline ||
         state == mojom::NetworkState::kConnected);
  if (state == mojom::NetworkState::kOnline) {
    return "Online";
  }

  return "Connected";
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

std::string NetworkingLog::GetContents() const {
  std::stringstream output;

  if (latest_network_info_) {
    output << kNetworkingInfoSectionName << kNewline << kNetworkNameTitle
           << latest_network_info_->name << kNewline << kNetworkTypeTitle
           << GetNetworkType(latest_network_info_->type) << kNewline
           << kNetworkStateTitle
           << GetNetworkStateString(latest_network_info_->state) << kNewline
           << kMacAddressTitle << latest_network_info_->mac_address.value_or("")
           << kNewline;

    if (latest_network_info_->type_properties) {
      AddTypePropertiesToLog(*latest_network_info_->type_properties,
                             latest_network_info_->type, output);
    }

    if (latest_network_info_->ip_config) {
      AddIPConfigPropertiesToLog(*latest_network_info_->ip_config, output);
    }
  }

  return output.str();
}

void NetworkingLog::UpdateContents(mojom::NetworkPtr latest_network_info) {
  latest_network_info_ = std::move(latest_network_info);
}

}  // namespace diagnostics
}  // namespace ash
