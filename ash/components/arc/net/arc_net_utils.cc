// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/net/arc_net_utils.h"

#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/device_state.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/onc/network_onc_utils.h"
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

// Parses a shill IPConfig dictionary and adds the relevant fields to
// the given |network| NetworkConfiguration object.
void AddIpConfiguration(arc::mojom::NetworkConfiguration* network,
                        const base::Value::Dict* shill_ipconfig) {
  // Only set the IP address and gateway if both are defined and non empty.
  const auto* address = shill_ipconfig->FindString(shill::kAddressProperty);
  const auto* gateway = shill_ipconfig->FindString(shill::kGatewayProperty);
  const int prefixlen =
      shill_ipconfig->FindInt(shill::kPrefixlenProperty).value_or(0);
  if (address && !address->empty() && gateway && !gateway->empty()) {
    if (prefixlen < 64) {
      network->host_ipv4_prefix_length = prefixlen;
      network->host_ipv4_address = *address;
      network->host_ipv4_gateway = *gateway;
    } else {
      network->host_ipv6_prefix_length = prefixlen;
      network->host_ipv6_global_addresses->push_back(*address);
      network->host_ipv6_gateway = *gateway;
    }
  }

  // If the user has overridden DNS with the "Google nameservers" UI options,
  // the kStaticIPConfigProperty object will be empty except for DNS addresses.
  if (const auto* dns_list =
          shill_ipconfig->FindList(shill::kNameServersProperty)) {
    for (const auto& dns_value : *dns_list) {
      const std::string& dns = dns_value.GetString();
      if (dns.empty()) {
        continue;
      }

      // When manually setting DNS, up to 4 addresses can be specified in the
      // UI. Unspecified entries can show up as 0.0.0.0 and should be removed.
      if (dns == "0.0.0.0") {
        continue;
      }

      network->host_dns_addresses->push_back(dns);
    }
  }

  if (const auto* domains =
          shill_ipconfig->FindList(shill::kSearchDomainsProperty)) {
    for (const auto& domain : *domains) {
      network->host_search_domains->push_back(domain.GetString());
    }
  }

  const int mtu = shill_ipconfig->FindInt(shill::kMtuProperty).value_or(0);
  if (mtu > 0) {
    network->host_mtu = mtu;
  }

  if (const auto* include_routes_list =
          shill_ipconfig->FindList(shill::kIncludedRoutesProperty)) {
    for (const auto& include_routes_value : *include_routes_list) {
      const std::string& include_route = include_routes_value.GetString();
      if (!include_route.empty()) {
        network->include_routes->push_back(include_route);
      }
    }
  }

  if (const auto* exclude_routes_list =
          shill_ipconfig->FindList(shill::kExcludedRoutesProperty)) {
    for (const auto& exclude_routes_value : *exclude_routes_list) {
      const std::string& exclude_route = exclude_routes_value.GetString();
      if (!exclude_route.empty()) {
        network->exclude_routes->push_back(exclude_route);
      }
    }
  }
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

}  // namespace

namespace arc::net_utils {

arc::mojom::NetworkConfigurationPtr TranslateNetworkProperties(
    const ash::NetworkState* network_state,
    const base::Value::Dict* shill_dict) {
  auto mojo = arc::mojom::NetworkConfiguration::New();
  // Initialize optional array fields to avoid null guards both here and in ARC.
  mojo->host_ipv6_global_addresses = std::vector<std::string>();
  mojo->host_search_domains = std::vector<std::string>();
  mojo->host_dns_addresses = std::vector<std::string>();
  mojo->include_routes = std::vector<std::string>();
  mojo->exclude_routes = std::vector<std::string>();
  mojo->connection_state =
      TranslateConnectionState(network_state->connection_state());
  mojo->guid = network_state->guid();
  if (mojo->guid.empty()) {
    NET_LOG(ERROR) << "Missing GUID property for network "
                   << network_state->path();
  }
  mojo->type = TranslateNetworkType(network_state->type());
  mojo->is_metered =
      shill_dict &&
      shill_dict->FindBool(shill::kMeteredProperty).value_or(false);

  // IP configuration data is added from the properties of the underlying shill
  // Device and shill Service attached to the Device. Device properties are
  // preferred because Service properties cannot have both IPv4 and IPv6
  // configurations at the same time for dual stack networks. It is necessary to
  // fallback on Service properties for networks without a shill Device exposed
  // over DBus (builtin OpenVPN, builtin L2TP client, Chrome extension VPNs),
  // particularly to obtain the DNS server list (b/155129178).
  // A connecting or newly connected network may not immediately have any
  // usable IP config object if IPv4 dhcp or IPv6 autoconf have not completed
  // yet. This case is covered by requesting shill properties asynchronously
  // when ash::NetworkStateHandlerObserver::NetworkPropertiesUpdated is
  // called.

  // Add shill's Device properties to the given mojo NetworkConfiguration
  // objects. This adds the network interface and current IP configurations.
  if (const auto* device =
          GetStateHandler()->GetDeviceState(network_state->device_path())) {
    mojo->network_interface = device->interface();
    for (const auto [key, value] : device->ip_configs()) {
      if (value.is_dict()) {
        AddIpConfiguration(mojo.get(), &value.GetDict());
      }
    }
  }

  if (shill_dict) {
    for (const auto* property :
         {shill::kStaticIPConfigProperty, shill::kSavedIPConfigProperty}) {
      const base::Value::Dict* config = shill_dict->FindDict(property);
      if (config) {
        AddIpConfiguration(mojo.get(), config);
      }
    }
  }

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

  return mojo;
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
      (state == shill::kStateDisconnect) || (state == "")) {
    return arc::mojom::ConnectionStateType::NOT_CONNECTED;
  }
  if (ash::NetworkState::StateIsPortalled(state)) {
    return arc::mojom::ConnectionStateType::PORTAL;
  }

  if (state == shill::kStateOnline) {
    return arc::mojom::ConnectionStateType::ONLINE;
  }

  // The remaining cases defined in shill dbus-constants are legacy values from
  // Flimflam and are not expected to be encountered. These are: kStateCarrier,
  // and kStateOffline.
  NOTREACHED() << "Unknown connection state: " << state;
  return arc::mojom::ConnectionStateType::NOT_CONNECTED;
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
  return arc::mojom::NetworkType::ETHERNET;
}

std::vector<arc::mojom::NetworkConfigurationPtr> TranslateNetworkStates(
    const std::string& arc_vpn_path,
    const ash::NetworkStateHandler::NetworkStateList& network_states,
    const std::map<std::string, base::Value::Dict>& shill_network_properties,
    const std::vector<patchpanel::NetworkDevice>& devices) {
  // Move the devices vector to a map keyed by its physical interface name in
  // order to avoid multiple loops. The map also filters non-ARC devices.
  std::map<std::string, patchpanel::NetworkDevice> arc_devices;
  for (const auto& d : devices) {
    if (d.guest_type() != patchpanel::NetworkDevice::ARC &&
        d.guest_type() != patchpanel::NetworkDevice::ARCVM) {
      continue;
    }
    arc_devices.emplace(d.phys_ifname(), d);
  }

  std::vector<arc::mojom::NetworkConfigurationPtr> networks;
  for (const ash::NetworkState* state : network_states) {
    const std::string& network_path = state->path();
    // Never tell Android about its own VPN.
    if (network_path == arc_vpn_path) {
      continue;
    }

    // For tethered networks, the underlying WiFi networks are not part of
    // active networks. Replace any such tethered network with its underlying
    // backing network, because ARC cannot match its datapath with the tethered
    // network configuration.
    state = GetShillBackedNetwork(state);
    if (!state) {
      continue;
    }

    const auto it = shill_network_properties.find(network_path);
    const base::Value::Dict* shill_dict =
        (it != shill_network_properties.end()) ? &it->second : nullptr;
    auto network = TranslateNetworkProperties(state, shill_dict);
    network->is_default_network = state == GetStateHandler()->DefaultNetwork();
    network->service_name = network_path;

    // Fill in ARC properties.
    auto arc_it =
        arc_devices.find(network->network_interface.value_or(std::string()));
    if (arc_it != arc_devices.end()) {
      network->arc_network_interface = arc_it->second.guest_ifname();
      network->arc_ipv4_address =
          IPv4AddressToString(arc_it->second.ipv4_addr());
      network->arc_ipv4_gateway =
          IPv4AddressToString(arc_it->second.host_ipv4_addr());
      network->arc_ipv4_prefix_length =
          arc_it->second.ipv4_subnet().prefix_len();
      // Fill in DNS proxy addresses.
      network->dns_proxy_addresses = std::vector<std::string>();
      if (arc_it->second.dns_proxy_ipv4_addr().length() > 0) {
        auto dns_proxy_ipv4_addr = PackedIPAddressToString(
            AF_INET, arc_it->second.dns_proxy_ipv4_addr());
        if (!dns_proxy_ipv4_addr.empty()) {
          network->dns_proxy_addresses->push_back(dns_proxy_ipv4_addr);
        }
      }
      if (arc_it->second.dns_proxy_ipv6_addr().length() > 0) {
        auto dns_proxy_ipv6_addr = PackedIPAddressToString(
            AF_INET6, arc_it->second.dns_proxy_ipv6_addr());
        if (!dns_proxy_ipv6_addr.empty()) {
          network->dns_proxy_addresses->push_back(dns_proxy_ipv6_addr);
        }
      }
    }
    networks.push_back(std::move(network));
  }
  return networks;
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

}  // namespace arc::net_utils
