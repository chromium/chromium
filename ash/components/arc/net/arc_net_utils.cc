// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/net/arc_net_utils.h"

#include <netinet/in.h>

#include "ash/components/arc/mojom/arc_wifi.mojom.h"
#include "ash/components/arc/mojom/net.mojom-shared.h"
#include "ash/components/arc/mojom/net.mojom.h"
#include "base/containers/map_util.h"
#include "base/strings/stringprintf.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/device_state.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/onc/network_onc_utils.h"
#include "net/base/ip_address.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace {

std::string PackedIPAddressToString(sa_family_t family,
                                    const std::string& data) {
  if (family != AF_INET && family != AF_INET6) {
    NET_LOG(ERROR) << "Invalid IP family " << family;
    return "";
  }
  if (family == AF_INET && data.length() != sizeof(in_addr)) {
    NET_LOG(ERROR) << "Invalid packed IPv4 data size " << data.length()
                   << ", expected " << sizeof(in_addr);
    return "";
  }
  if (family == AF_INET6 && data.length() != sizeof(in6_addr)) {
    NET_LOG(ERROR) << "Invalid packed IPv6 data size " << data.length()
                   << ", expected " << sizeof(in6_addr);
    return "";
  }

  char buf[INET6_ADDRSTRLEN] = {0};
  return !inet_ntop(family, data.data(), buf, sizeof(buf)) ? "" : buf;
}

ash::NetworkStateHandler* GetStateHandler() {
  return ash::NetworkHandler::Get()->network_state_handler();
}

// Update the IP configuration fields in the given |mojo| NetworkConfiguration
// object with |network_state|.
void UpdateIpConfiguration(const ash::NetworkState& network_state,
                           arc::mojom::NetworkConfiguration* mojo) {
  const ash::NetworkConfig* config = network_state.network_config();

  // Service may not be connected.
  if (!config) {
    return;
  }

  // Only set the IP address and gateway if both are defined.
  if (config->ipv4_address && config->ipv4_gateway) {
    mojo->host_ipv4_address = config->ipv4_address->addr.ToString();
    mojo->host_ipv4_prefix_length = config->ipv4_address->prefix_len;
    mojo->host_ipv4_gateway = config->ipv4_gateway->ToString();
  }
  if (config->ipv6_addresses.size() > 0 && config->ipv6_gateway) {
    for (const auto& cidr : config->ipv6_addresses) {
      mojo->host_ipv6_global_addresses->push_back(cidr.addr.ToString());
    }
    // Assume that all addresses have the same prefix length.
    mojo->host_ipv6_prefix_length = config->ipv6_addresses[0].prefix_len;
    mojo->host_ipv6_gateway = config->ipv6_gateway->ToString();
  }

  for (const auto& dns : config->dns_servers) {
    mojo->host_dns_addresses->push_back(dns.ToString());
  }
  for (const auto& domain : config->search_domains) {
    mojo->host_search_domains->push_back(domain);
  }

  for (const auto& cidr : config->included_routes) {
    mojo->include_routes->push_back(base::StringPrintf(
        "%s/%d", cidr.addr.ToString().c_str(), cidr.prefix_len));
  }
  for (const auto& cidr : config->excluded_routes) {
    mojo->exclude_routes->push_back(base::StringPrintf(
        "%s/%d", cidr.addr.ToString().c_str(), cidr.prefix_len));
  }

  mojo->host_mtu = config->mtu > 0 ? config->mtu : 0;
}

const ash::NetworkState* GetShillBackedNetwork(
    const ash::NetworkState* network) {
  if (!network) {
    return nullptr;
  }

  // Non-Tether networks are already backed by Shill.
  const std::string type = network->type();
  if (type.empty() || !ash::NetworkTypePattern::Tether().MatchesType(type)) {
    return network;
  }

  // Tether networks which are not connected are also not backed by Shill.
  if (!network->IsConnectedState()) {
    return nullptr;
  }

  // Connected Tether networks delegate to an underlying Wi-Fi network.
  DCHECK(!network->tether_guid().empty());
  return GetStateHandler()->GetNetworkStateFromGuid(network->tether_guid());
}

std::string IPv4AddressToString(uint32_t addr) {
  char buf[INET_ADDRSTRLEN] = {0};
  struct in_addr ia;
  ia.s_addr = addr;
  return !inet_ntop(AF_INET, &ia, buf, sizeof(buf)) ? std::string() : buf;
}

std::string KeyManagementMethodToString(arc::mojom::KeyManagement management) {
  switch (management) {
    case arc::mojom::KeyManagement::kIeee8021X:
      return "WEP-8021X";
    case arc::mojom::KeyManagement::kFtEap:
      return "FT-EAP";
    case arc::mojom::KeyManagement::kFtPsk:
      return "FT-PSK";
    case arc::mojom::KeyManagement::kFtSae:
      return "FT-SAE";
    case arc::mojom::KeyManagement::kWpaEap:
      return "WPA-EAP";
    case arc::mojom::KeyManagement::kWpaEapSha256:
      return "WPA-EAP-SHA256";
    case arc::mojom::KeyManagement::kWpaPsk:
      return "WPA-PSK";
    case arc::mojom::KeyManagement::kSae:
      return "SAE";
    case arc::mojom::KeyManagement::kNone:
      return "None";
  }
  return "unknown";
}

patchpanel::SocketConnectionEvent::IpProtocol TranslateIpProtocol(
    arc::mojom::IpProtocol proto) {
  switch (proto) {
    case arc::mojom::IpProtocol::kTcp:
      return patchpanel::SocketConnectionEvent::IpProtocol::
          SocketConnectionEvent_IpProtocol_TCP;
    case arc::mojom::IpProtocol::kUdp:
      return patchpanel::SocketConnectionEvent::IpProtocol::
          SocketConnectionEvent_IpProtocol_UDP;
    case arc::mojom::IpProtocol::kUnknown:
      NET_LOG(ERROR) << "Unknown IP protocol";
      return patchpanel::SocketConnectionEvent::IpProtocol::
          SocketConnectionEvent_IpProtocol_UNKNOWN_PROTO;
  }
}

patchpanel::SocketConnectionEvent::SocketEvent TranslateSocketEvent(
    arc::mojom::SocketEvent event) {
  switch (event) {
    case arc::mojom::SocketEvent::kOpen:
      return patchpanel::SocketConnectionEvent::SocketEvent::
          SocketConnectionEvent_SocketEvent_OPEN;
    case arc::mojom::SocketEvent::kClose:
      return patchpanel::SocketConnectionEvent::SocketEvent::
          SocketConnectionEvent_SocketEvent_CLOSE;
    case arc::mojom::SocketEvent::kUnknown:
      NET_LOG(ERROR) << "Unknown socket event";
      return patchpanel::SocketConnectionEvent::SocketEvent::
          SocketConnectionEvent_SocketEvent_UNKNOWN_EVENT;
  }
}

patchpanel::SocketConnectionEvent::QosCategory TranslateQosCategory(
    arc::mojom::QosCategory category) {
  switch (category) {
    case arc::mojom::QosCategory::kRealtimeInteractive:
      return patchpanel::SocketConnectionEvent::QosCategory::
          SocketConnectionEvent_QosCategory_REALTIME_INTERACTIVE;
    case arc::mojom::QosCategory::kMultimediaConferencing:
      return patchpanel::SocketConnectionEvent::QosCategory::
          SocketConnectionEvent_QosCategory_MULTIMEDIA_CONFERENCING;
    case arc::mojom::QosCategory::kUnknown:
      return patchpanel::SocketConnectionEvent::QosCategory::
          SocketConnectionEvent_QosCategory_UNKNOWN_CATEGORY;
  }
}

}  // namespace

namespace arc::net_utils {

void FillConfigurationsFromState(const ash::NetworkState* network_state,
                                 const base::Value::Dict* shill_dict,
                                 arc::mojom::NetworkConfiguration* mojo) {
  // Initialize optional array fields to avoid null guards both here and in ARC.
  mojo->host_ipv6_global_addresses = std::vector<std::string>();
  mojo->host_search_domains = std::vector<std::string>();
  mojo->host_dns_addresses = std::vector<std::string>();
  mojo->include_routes = std::vector<std::string>();
  mojo->exclude_routes = std::vector<std::string>();
  mojo->connection_state =
      TranslateConnectionState(network_state->connection_state());
  mojo->guid = network_state->guid();
  mojo->is_default_network =
      network_state == GetStateHandler()->DefaultNetwork();
  mojo->service_name = network_state->path();
  if (mojo->guid.empty()) {
    NET_LOG(ERROR) << "Missing GUID property for network "
                   << network_state->path();
  }
  mojo->type = TranslateNetworkType(network_state->type());
  mojo->is_metered =
      shill_dict &&
      shill_dict->FindBool(shill::kMeteredProperty).value_or(false);

  // Add shill's Device properties to the given mojo NetworkConfiguration
  // objects. This adds the network interface.
  if (const auto* device =
          GetStateHandler()->GetDeviceState(network_state->device_path())) {
    mojo->network_interface = device->interface();
  }

  UpdateIpConfiguration(*network_state, mojo);

  if (mojo->type == arc::mojom::NetworkType::WIFI) {
    mojo->wifi = arc::mojom::WiFi::New();
    mojo->wifi->bssid = network_state->bssid();
    mojo->wifi->hex_ssid = network_state->GetHexSsid();
    mojo->wifi->security =
        TranslateWiFiSecurity(network_state->security_class());
    mojo->wifi->frequency = network_state->frequency();
    mojo->wifi->signal_strength = network_state->signal_strength();
    mojo->wifi->rssi = network_state->rssi();
    if (shill_dict) {
      mojo->wifi->hidden_ssid =
          shill_dict->FindBoolByDottedPath(shill::kWifiHiddenSsid)
              .value_or(false);
      const auto* fqdn =
          shill_dict->FindStringByDottedPath(shill::kPasspointFQDNProperty);
      if (fqdn && !fqdn->empty()) {
        mojo->wifi->fqdn = *fqdn;
      }
    }
  }
}

arc::mojom::WifiScanResultPtr NetworkStateToWifiScanResult(
    const ash::NetworkState& network_state) {
  auto mojo = arc::mojom::WifiScanResult::New();
  mojo->bssid = network_state.bssid();
  mojo->hex_ssid = network_state.GetHexSsid();
  mojo->security = TranslateWiFiSecurity(network_state.security_class());
  mojo->frequency = network_state.frequency();
  mojo->rssi = network_state.rssi();
  return mojo;
}

void FillConfigurationsFromDevice(const patchpanel::NetworkDevice& device,
                                  arc::mojom::NetworkConfiguration* mojo) {
  mojo->network_interface = device.phys_ifname();
  mojo->arc_network_interface = device.guest_ifname();
  mojo->arc_ipv4_address = IPv4AddressToString(device.ipv4_addr());
  mojo->arc_ipv4_gateway = IPv4AddressToString(device.host_ipv4_addr());
  mojo->arc_ipv4_prefix_length = device.ipv4_subnet().prefix_len();
  // Fill in DNS proxy addresses.
  mojo->dns_proxy_addresses = std::vector<std::string>();
  if (!device.dns_proxy_ipv4_addr().empty()) {
    auto dns_proxy_ipv4_addr =
        PackedIPAddressToString(AF_INET, device.dns_proxy_ipv4_addr());
    mojo->dns_proxy_addresses->emplace_back(dns_proxy_ipv4_addr);
  }
  if (!device.dns_proxy_ipv6_addr().empty()) {
    auto dns_proxy_ipv6_addr =
        PackedIPAddressToString(AF_INET6, device.dns_proxy_ipv6_addr());
    mojo->dns_proxy_addresses->emplace_back(dns_proxy_ipv6_addr);
  }
  // Assign the technology of the physical device the virtual device is tied to.
  switch (device.technology_type()) {
    case patchpanel::NetworkDevice::CELLULAR:
      mojo->type = mojom::NetworkType::CELLULAR;
      break;
    case patchpanel::NetworkDevice::ETHERNET:
      mojo->type = mojom::NetworkType::ETHERNET;
      break;
    case patchpanel::NetworkDevice::WIFI:
      mojo->type = mojom::NetworkType::WIFI;
      break;
    default:
      break;
  }
}

std::string TranslateEapMethod(arc::mojom::EapMethod method) {
  switch (method) {
    case arc::mojom::EapMethod::kLeap:
      return shill::kEapMethodLEAP;
    case arc::mojom::EapMethod::kPeap:
      return shill::kEapMethodPEAP;
    case arc::mojom::EapMethod::kTls:
      return shill::kEapMethodTLS;
    case arc::mojom::EapMethod::kTtls:
      return shill::kEapMethodTTLS;
    case arc::mojom::EapMethod::kNone:
      return "";
  }
  NET_LOG(ERROR) << "Unknown EAP method";
  return "";
}

std::string TranslateEapPhase2Method(arc::mojom::EapPhase2Method method) {
  switch (method) {
    case arc::mojom::EapPhase2Method::kPap:
      return shill::kEapPhase2AuthTTLSPAP;
    case arc::mojom::EapPhase2Method::kMschap:
      return shill::kEapPhase2AuthTTLSMSCHAP;
    case arc::mojom::EapPhase2Method::kMschapv2:
      return shill::kEapPhase2AuthTTLSMSCHAPV2;
    case arc::mojom::EapPhase2Method::kNone:
      return "";
  }
  NET_LOG(ERROR) << "Unknown EAP phase 2 method";
  return "";
}

std::string TranslateEapMethodToOnc(arc::mojom::EapMethod method) {
  switch (method) {
    case arc::mojom::EapMethod::kLeap:
      return onc::eap::kLEAP;
    case arc::mojom::EapMethod::kPeap:
      return onc::eap::kPEAP;
    case arc::mojom::EapMethod::kTls:
      return onc::eap::kEAP_TLS;
    case arc::mojom::EapMethod::kTtls:
      return onc::eap::kEAP_TTLS;
    case arc::mojom::EapMethod::kNone:
      return std::string();
  }
  NET_LOG(ERROR) << "Unknown EAP method: " << method;
  return std::string();
}

std::string TranslateEapPhase2MethodToOnc(arc::mojom::EapPhase2Method method) {
  switch (method) {
    case arc::mojom::EapPhase2Method::kPap:
      return onc::eap::kPAP;
    case arc::mojom::EapPhase2Method::kMschap:
      return onc::eap::kMSCHAP;
    case arc::mojom::EapPhase2Method::kMschapv2:
      return onc::eap::kMSCHAPv2;
    case arc::mojom::EapPhase2Method::kNone:
      return std::string();
  }
  NET_LOG(ERROR) << "Unknown EAP phase 2 method: " << method;
  return std::string();
}

std::string TranslateKeyManagement(mojom::KeyManagement management) {
  switch (management) {
    case arc::mojom::KeyManagement::kIeee8021X:
      return shill::kKeyManagementIEEE8021X;
    case arc::mojom::KeyManagement::kFtEap:
    case arc::mojom::KeyManagement::kFtPsk:
    case arc::mojom::KeyManagement::kFtSae:
    case arc::mojom::KeyManagement::kWpaEap:
    case arc::mojom::KeyManagement::kWpaEapSha256:
    case arc::mojom::KeyManagement::kWpaPsk:
    case arc::mojom::KeyManagement::kSae:
      // Currently these key managements are not handled.
      NET_LOG(ERROR) << "Key management is not supported";
      return "";
    case arc::mojom::KeyManagement::kNone:
      return "";
  }
  NET_LOG(ERROR) << "Unknown key management";
  return "";
}

std::string TranslateKeyManagementToOnc(mojom::KeyManagement management) {
  if (management == arc::mojom::KeyManagement::kIeee8021X) {
    return onc::wifi::kWEP_8021X;
  } else if (management == arc::mojom::KeyManagement::kNone) {
    return std::string();
  }
  // Currently other key managements are not handled.
  NET_LOG(ERROR) << "Key management is not supported or invalid: "
                 << KeyManagementMethodToString(management);
  return std::string();
}

arc::mojom::SecurityType TranslateWiFiSecurity(
    const std::string& security_class) {
  if (security_class == shill::kSecurityClassNone) {
    return arc::mojom::SecurityType::NONE;
  }

  if (security_class == shill::kSecurityClassWep) {
    return arc::mojom::SecurityType::WEP_PSK;
  }

  if (security_class == shill::kSecurityClassPsk) {
    return arc::mojom::SecurityType::WPA_PSK;
  }

  if (security_class == shill::kSecurityClass8021x) {
    return arc::mojom::SecurityType::WPA_EAP;
  }

  NET_LOG(ERROR) << "Unknown WiFi security class " << security_class;
  return arc::mojom::SecurityType::NONE;
}

arc::mojom::ConnectionStateType TranslateConnectionState(
    const std::string& state) {
  if (state == shill::kStateReady) {
    return arc::mojom::ConnectionStateType::CONNECTED;
  }

  if (state == shill::kStateAssociation ||
      state == shill::kStateConfiguration) {
    return arc::mojom::ConnectionStateType::CONNECTING;
  }

  if ((state == shill::kStateIdle) || (state == shill::kStateFailure) ||
      (state == shill::kStateDisconnecting) || (state == "")) {
    return arc::mojom::ConnectionStateType::NOT_CONNECTED;
  }
  if (ash::NetworkState::StateIsPortalled(state)) {
    return arc::mojom::ConnectionStateType::PORTAL;
  }

  if (state == shill::kStateOnline) {
    return arc::mojom::ConnectionStateType::ONLINE;
  }

  // The remaining cases defined in shill dbus-constants are legacy values from
  // Flimflam and are not expected to be encountered.
  NOTREACHED() << "Unknown connection state: " << state;
}

arc::mojom::NetworkType TranslateNetworkType(const std::string& type) {
  if (type == shill::kTypeWifi) {
    return arc::mojom::NetworkType::WIFI;
  }

  if (type == shill::kTypeVPN) {
    return arc::mojom::NetworkType::VPN;
  }

  if (type == shill::kTypeEthernet) {
    return arc::mojom::NetworkType::ETHERNET;
  }

  if (type == shill::kTypeEthernetEap) {
    return arc::mojom::NetworkType::ETHERNET;
  }

  if (type == shill::kTypeCellular) {
    return arc::mojom::NetworkType::CELLULAR;
  }

  NOTREACHED() << "Unknown network type: " << type;
}

std::vector<arc::mojom::NetworkConfigurationPtr> TranslateNetworkDevices(
    const std::vector<patchpanel::NetworkDevice>& devices,
    const std::string& arc_vpn_path,
    const ash::NetworkStateHandler::NetworkStateList& active_network_states,
    const std::map<std::string, base::Value::Dict>& shill_network_properties) {
  // Map of interface name to a unique active network state on it, if such state
  // exists. Otherwise, report device without associated network states.
  std::map<std::string, arc::mojom::NetworkConfigurationPtr> ifname_config_map;
  std::vector<arc::mojom::NetworkConfigurationPtr> network_configs;

  for (const patchpanel::NetworkDevice& device : devices) {
    // Filter non-ARC devices.
    if (device.guest_type() != patchpanel::NetworkDevice::ARC &&
        device.guest_type() != patchpanel::NetworkDevice::ARCVM) {
      continue;
    }
    auto config_it = ifname_config_map
                         .try_emplace(device.phys_ifname(),
                                      arc::mojom::NetworkConfiguration::New())
                         .first;
    FillConfigurationsFromDevice(device, config_it->second.get());
    // Connection state default to NOT_CONNECTED unless there is active network
    // state on interface device.phys_ifname().
    config_it->second->connection_state =
        arc::mojom::ConnectionStateType::NOT_CONNECTED;
  }

  for (const ash::NetworkState* const state : active_network_states) {
    // Never tell Android about its own VPN.
    if (state->path() == arc_vpn_path) {
      continue;
    }
    // For networks established with instant tethering, the underlying WiFi
    // networks are not part of active networks. Replace any such tethered
    // network with its underlying backing network, because ARC cannot match its
    // datapath with the tethered network configuration. For other cases, the
    // underlying_state is identical to state.
    const ash::NetworkState* const underlying_state =
        GetShillBackedNetwork(state);
    if (!underlying_state) {
      continue;
    }

    std::string if_name;
    if (const auto* device = GetStateHandler()->GetDeviceState(
            underlying_state->device_path())) {
      if_name = device->interface();
    }
    const base::Value::Dict* shill_dict =
        base::FindOrNull(shill_network_properties, underlying_state->path());

    if (if_name.empty()) {
      // Add active network without network_interface (e.g.: host VPN).
      auto network = arc::mojom::NetworkConfiguration::New();
      FillConfigurationsFromState(underlying_state, shill_dict, network.get());
      network_configs.push_back(std::move(network));
    } else {
      const auto itr = ifname_config_map.find(if_name);
      if (itr != ifname_config_map.end()) {
        FillConfigurationsFromState(underlying_state, shill_dict,
                                    itr->second.get());
      } else {
        NET_LOG(ERROR) << "Interface " << if_name << " does not have a device.";
      }
    }
  }

  for (auto& network_config : ifname_config_map) {
    if (network_config.second->arc_network_interface.has_value()) {
      network_configs.push_back(std::move(network_config.second));
    }
  }
  return network_configs;
}

std::vector<arc::mojom::NetworkConfigurationPtr> TranslateNetworkStates(
    const std::string& arc_vpn_path,
    const ash::NetworkStateHandler::NetworkStateList& network_states,
    const std::map<std::string, base::Value::Dict>& shill_network_properties) {
  std::vector<arc::mojom::NetworkConfigurationPtr> networks;
  for (const ash::NetworkState* const state : network_states) {
    // Never tell Android about its own VPN.
    if (state->path() == arc_vpn_path) {
      continue;
    }
    // For networks established with instant tethering, the underlying WiFi
    // networks are not part of active networks. Replace any such tethered
    // network with its underlying backing network, because ARC cannot match its
    // datapath with the tethered network configuration. For other cases, the
    // underlying_state is identical to state.
    const ash::NetworkState* const underlying_state =
        GetShillBackedNetwork(state);
    if (!underlying_state) {
      continue;
    }

    const auto it = shill_network_properties.find(underlying_state->path());
    const base::Value::Dict* shill_dict =
        (it != shill_network_properties.end()) ? &it->second : nullptr;
    auto network = arc::mojom::NetworkConfiguration::New();
    FillConfigurationsFromState(underlying_state, shill_dict, network.get());
    networks.push_back(std::move(network));
  }
  return networks;
}

std::vector<arc::mojom::WifiScanResultPtr> TranslateScanResults(
    const ash::NetworkStateHandler::NetworkStateList& network_states) {
  std::vector<arc::mojom::WifiScanResultPtr> results;
  for (const ash::NetworkState* const state : network_states) {
    if (state->GetNetworkTechnologyType() !=
        ash::NetworkState::NetworkTechnologyType::kWiFi) {
      continue;
    }

    results.push_back(NetworkStateToWifiScanResult(*state));
  }
  return results;
}

base::Value::List TranslateSubjectNameMatchListToValue(
    const std::vector<std::string>& string_list) {
  base::Value::List result;
  for (const auto& item : string_list) {
    // Type and value is separated by ":" in vector, separate them by splitting
    // at ":".
    int idx = item.find(":");
    std::string type = item.substr(0, idx);
    std::string value = item.substr(idx + 1, item.size());
    base::Value::Dict entry;
    entry.Set(::onc::eap_subject_alternative_name_match::kType, type);
    entry.Set(::onc::eap_subject_alternative_name_match::kValue, value);
    result.Append(std::move(entry));
  }
  return result;
}

std::unique_ptr<patchpanel::SocketConnectionEvent>
TranslateSocketConnectionEvent(const mojom::SocketConnectionEventPtr& mojom) {
  std::unique_ptr<patchpanel::SocketConnectionEvent> msg =
      std::make_unique<patchpanel::SocketConnectionEvent>();

  msg->set_saddr(
      std::string{reinterpret_cast<const char*>(mojom->src_addr.bytes().data()),
                  mojom->src_addr.size()});
  msg->set_daddr(reinterpret_cast<const char*>(mojom->dst_addr.bytes().data()),
                 mojom->dst_addr.size());

  msg->set_sport(mojom->src_port);
  msg->set_dport(mojom->dst_port);

  if (const auto protocol = TranslateIpProtocol(mojom->proto);
      protocol != patchpanel::SocketConnectionEvent::IpProtocol::
                      SocketConnectionEvent_IpProtocol_UNKNOWN_PROTO) {
    msg->set_proto(protocol);
  } else {
    NET_LOG(ERROR) << "IP protocol is unknown, translate socket connection "
                      "event failed. IP protocol is: "
                   << mojom->proto;
    return nullptr;
  }

  if (const auto event = TranslateSocketEvent(mojom->event);
      event != patchpanel::SocketConnectionEvent::SocketEvent::
                   SocketConnectionEvent_SocketEvent_UNKNOWN_EVENT) {
    msg->set_event(event);
  } else {
    NET_LOG(ERROR) << "Socket event is unknown, translate socket connection "
                      "event failed. Socket event is: "
                   << mojom->event;
    return nullptr;
  }

  if (const auto category = TranslateQosCategory(mojom->qos_category)) {
    msg->set_category(category);
  }

  return msg;
}

bool AreConfigurationsEquivalent(
    std::vector<arc::mojom::NetworkConfigurationPtr>& latest_networks,
    std::vector<arc::mojom::NetworkConfigurationPtr>& cached_networks) {
  if (cached_networks.size() != latest_networks.size()) {
    return false;
  }

  const auto arc_iface_compare =
      [](const arc::mojom::NetworkConfigurationPtr& a,
         const arc::mojom::NetworkConfigurationPtr& b) -> bool {
    return a->arc_network_interface > b->arc_network_interface;
  };
  std::sort(cached_networks.begin(), cached_networks.end(), arc_iface_compare);
  std::sort(latest_networks.begin(), latest_networks.end(), arc_iface_compare);

  for (size_t i = 0; i < latest_networks.size(); ++i) {
    const arc::mojom::NetworkConfigurationPtr& latest = latest_networks.at(i);
    const arc::mojom::NetworkConfigurationPtr& cached = cached_networks.at(i);

    if (latest->arc_network_interface != cached->arc_network_interface ||
        latest->guid != cached->guid ||
        latest->connection_state != cached->connection_state ||
        latest->is_default_network != cached->is_default_network ||
        latest->type != cached->type ||
        latest->is_metered != cached->is_metered ||
        latest->network_interface != cached->network_interface ||
        latest->host_mtu != cached->host_mtu ||
        latest->host_dns_addresses != cached->host_dns_addresses ||
        latest->dns_proxy_addresses != cached->dns_proxy_addresses ||
        latest->host_search_domains != cached->host_search_domains ||
        latest->host_ipv4_address != cached->host_ipv4_address ||
        latest->host_ipv6_global_addresses !=
            cached->host_ipv6_global_addresses) {
      return false;
    }
  }

  return true;
}
}  // namespace arc::net_utils
