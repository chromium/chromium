// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/net/arc_net_host_impl.h"

#include <net/if.h>

#include <utility>

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/net/cert_manager.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "base/containers/cxx20_erase.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/singleton.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "chromeos/ash/components/dbus/patchpanel/patchpanel_client.h"
#include "chromeos/ash/components/dbus/patchpanel/patchpanel_service.pb.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/client_cert_util.h"
#include "chromeos/ash/components/network/device_state.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_connection_handler.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "chromeos/ash/components/network/onc/network_onc_utils.h"
#include "components/device_event_log/device_event_log.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace {

constexpr int kGetNetworksListLimit = 100;

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

ash::ManagedNetworkConfigurationHandler* GetManagedConfigurationHandler() {
  return ash::NetworkHandler::Get()->managed_network_configuration_handler();
}

ash::NetworkConnectionHandler* GetNetworkConnectionHandler() {
  return ash::NetworkHandler::Get()->network_connection_handler();
}

ash::NetworkProfileHandler* GetNetworkProfileHandler() {
  return ash::NetworkHandler::Get()->network_profile_handler();
}

const ash::NetworkProfile* GetNetworkProfile() {
  return GetNetworkProfileHandler()->GetProfileForUserhash(
      ash::LoginState::Get()->primary_user_hash());
}

std::vector<const ash::NetworkState*> GetHostActiveNetworks() {
  std::vector<const ash::NetworkState*> active_networks;
  GetStateHandler()->GetActiveNetworkListByType(
      ash::NetworkTypePattern::Default(), &active_networks);
  return active_networks;
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

std::string TranslateKeyManagement(arc::mojom::KeyManagement management) {
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

arc::mojom::SecurityType TranslateWiFiSecurity(
    const std::string& security_class) {
  if (security_class == shill::kSecurityClassNone)
    return arc::mojom::SecurityType::NONE;

  if (security_class == shill::kSecurityClassWep)
    return arc::mojom::SecurityType::WEP_PSK;

  if (security_class == shill::kSecurityClassPsk)
    return arc::mojom::SecurityType::WPA_PSK;

  if (security_class == shill::kSecurityClass8021x)
    return arc::mojom::SecurityType::WPA_EAP;

  NET_LOG(ERROR) << "Unknown WiFi security class " << security_class;
  return arc::mojom::SecurityType::NONE;
}

// Translates a shill connection state into a mojo ConnectionStateType.
// This is effectively the inverse function of shill.Service::GetStateString
// defined in platform2/shill/service.cc, with in addition some of shill's
// connection states translated to the same ConnectionStateType value.
arc::mojom::ConnectionStateType TranslateConnectionState(
    const std::string& state) {
  if (state == shill::kStateReady)
    return arc::mojom::ConnectionStateType::CONNECTED;

  if (state == shill::kStateAssociation || state == shill::kStateConfiguration)
    return arc::mojom::ConnectionStateType::CONNECTING;

  if ((state == shill::kStateIdle) || (state == shill::kStateFailure) ||
      (state == shill::kStateDisconnect) || (state == "")) {
    return arc::mojom::ConnectionStateType::NOT_CONNECTED;
  }
  if (ash::NetworkState::StateIsPortalled(state))
    return arc::mojom::ConnectionStateType::PORTAL;

  if (state == shill::kStateOnline)
    return arc::mojom::ConnectionStateType::ONLINE;

  // The remaining cases defined in shill dbus-constants are legacy values from
  // Flimflam and are not expected to be encountered. These are: kStateCarrier,
  // and kStateOffline.
  NOTREACHED() << "Unknown connection state: " << state;
  return arc::mojom::ConnectionStateType::NOT_CONNECTED;
}

bool IsActiveNetworkState(const ash::NetworkState* network) {
  if (!network)
    return false;

  const std::string& state = network->connection_state();
  return state == shill::kStateReady || state == shill::kStateOnline ||
         state == shill::kStateAssociation ||
         state == shill::kStateConfiguration ||
         state == shill::kStateNoConnectivity ||
         state == shill::kStateRedirectFound ||
         state == shill::kStatePortalSuspected;
}

arc::mojom::NetworkType TranslateNetworkType(const std::string& type) {
  if (type == shill::kTypeWifi)
    return arc::mojom::NetworkType::WIFI;

  if (type == shill::kTypeVPN)
    return arc::mojom::NetworkType::VPN;

  if (type == shill::kTypeEthernet)
    return arc::mojom::NetworkType::ETHERNET;

  if (type == shill::kTypeEthernetEap)
    return arc::mojom::NetworkType::ETHERNET;

  if (type == shill::kTypeCellular)
    return arc::mojom::NetworkType::CELLULAR;

  NOTREACHED() << "Unknown network type: " << type;
  return arc::mojom::NetworkType::ETHERNET;
}

// Parses a shill IPConfig dictionary and adds the relevant fields to
// the given |network| NetworkConfiguration object.
void AddIpConfiguration(arc::mojom::NetworkConfiguration* network,
                        const base::Value* shill_ipconfig) {
  const base::Value::Dict* shill_ipconfig_dict = shill_ipconfig->GetIfDict();
  if (!shill_ipconfig_dict)
    return;

  // Only set the IP address and gateway if both are defined and non empty.
  const auto* address =
      shill_ipconfig_dict->FindString(shill::kAddressProperty);
  const auto* gateway =
      shill_ipconfig_dict->FindString(shill::kGatewayProperty);
  const int prefixlen =
      shill_ipconfig_dict->FindInt(shill::kPrefixlenProperty).value_or(0);
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
          shill_ipconfig_dict->FindList(shill::kNameServersProperty)) {
    for (const auto& dns_value : *dns_list) {
      const std::string& dns = dns_value.GetString();
      if (dns.empty())
        continue;

      // When manually setting DNS, up to 4 addresses can be specified in the
      // UI. Unspecified entries can show up as 0.0.0.0 and should be removed.
      if (dns == "0.0.0.0")
        continue;

      network->host_dns_addresses->push_back(dns);
    }
  }

  if (const auto* domains =
          shill_ipconfig_dict->FindList(shill::kSearchDomainsProperty)) {
    for (const auto& domain : *domains)
      network->host_search_domains->push_back(domain.GetString());
  }

  const int mtu = shill_ipconfig_dict->FindInt(shill::kMtuProperty).value_or(0);
  if (mtu > 0)
    network->host_mtu = mtu;

  if (const auto* include_routes_list =
          shill_ipconfig_dict->FindList(shill::kIncludedRoutesProperty)) {
    for (const auto& include_routes_value : *include_routes_list) {
      const std::string& include_route = include_routes_value.GetString();
      if (!include_route.empty()) {
        network->include_routes->push_back(include_route);
      }
    }
  }

  if (const auto* exclude_routes_list =
          shill_ipconfig_dict->FindList(shill::kExcludedRoutesProperty)) {
    for (const auto& exclude_routes_value : *exclude_routes_list) {
      const std::string& exclude_route = exclude_routes_value.GetString();
      if (!exclude_route.empty()) {
        network->exclude_routes->push_back(exclude_route);
      }
    }
  }
}

arc::mojom::NetworkConfigurationPtr TranslateNetworkProperties(
    const ash::NetworkState* network_state,
    const base::Value* shill_dict) {
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
      shill_dict->FindBoolPath(shill::kMeteredProperty).value_or(false);

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
    for (const auto kv : device->ip_configs())
      AddIpConfiguration(mojo.get(), &kv.second);
  }

  if (shill_dict) {
    for (const auto* property :
         {shill::kStaticIPConfigProperty, shill::kSavedIPConfigProperty}) {
      AddIpConfiguration(mojo.get(), shill_dict->GetDict().Find(property));
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
          shill_dict->FindBoolPath(shill::kWifiHiddenSsid).value_or(false);
      const auto* fqdn =
          shill_dict->FindStringPath(shill::kPasspointFQDNProperty);
      if (fqdn && !fqdn->empty()) {
        mojo->wifi->fqdn = *fqdn;
      }
    }
  }

  return mojo;
}

const ash::NetworkState* GetShillBackedNetwork(
    const ash::NetworkState* network) {
  if (!network)
    return nullptr;

  // Non-Tether networks are already backed by Shill.
  const std::string type = network->type();
  if (type.empty() || !ash::NetworkTypePattern::Tether().MatchesType(type))
    return network;

  // Tether networks which are not connected are also not backed by Shill.
  if (!network->IsConnectedState())
    return nullptr;

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

// Convenience helper for translating a vector of NetworkState objects to a
// vector of mojo NetworkConfiguration objects.
std::vector<arc::mojom::NetworkConfigurationPtr> TranslateNetworkStates(
    const std::string& arc_vpn_path,
    const ash::NetworkStateHandler::NetworkStateList& network_states,
    const std::map<std::string, base::Value>& shill_network_properties,
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
    if (network_path == arc_vpn_path)
      continue;

    // For tethered networks, the underlying WiFi networks are not part of
    // active networks. Replace any such tethered network with its underlying
    // backing network, because ARC cannot match its datapath with the tethered
    // network configuration.
    state = GetShillBackedNetwork(state);
    if (!state)
      continue;

    const auto it = shill_network_properties.find(network_path);
    const auto* shill_dict =
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

void ForgetNetworkSuccessCallback(
    base::OnceCallback<void(arc::mojom::NetworkResult)> callback) {
  std::move(callback).Run(arc::mojom::NetworkResult::SUCCESS);
}

void ForgetNetworkFailureCallback(
    base::OnceCallback<void(arc::mojom::NetworkResult)> callback,
    const std::string& error_name) {
  std::move(callback).Run(arc::mojom::NetworkResult::FAILURE);
}

void StartConnectSuccessCallback(
    base::OnceCallback<void(arc::mojom::NetworkResult)> callback) {
  std::move(callback).Run(arc::mojom::NetworkResult::SUCCESS);
}

void StartConnectFailureCallback(
    base::OnceCallback<void(arc::mojom::NetworkResult)> callback,
    const std::string& error_name) {
  std::move(callback).Run(arc::mojom::NetworkResult::FAILURE);
}

void StartDisconnectSuccessCallback(
    base::OnceCallback<void(arc::mojom::NetworkResult)> callback) {
  std::move(callback).Run(arc::mojom::NetworkResult::SUCCESS);
}

void StartDisconnectFailureCallback(
    base::OnceCallback<void(arc::mojom::NetworkResult)> callback,
    const std::string& error_name) {
  std::move(callback).Run(arc::mojom::NetworkResult::FAILURE);
}

void HostVpnSuccessCallback() {}

void HostVpnErrorCallback(const std::string& operation,
                          const std::string& error_name) {
  NET_LOG(ERROR) << "HostVpnErrorCallback: " << operation << ": " << error_name;
}

void ArcVpnSuccessCallback() {}

void ArcVpnErrorCallback(const std::string& operation,
                         const std::string& error_name) {
  NET_LOG(ERROR) << "ArcVpnErrorCallback: " << operation << ": " << error_name;
}

void AddPasspointCredentialsFailureCallback(const std::string& error_name,
                                            const std::string& error_message) {
  NET_LOG(ERROR) << "Failed to add passpoint credentials, error:" << error_name
                 << ", message: " << error_message;
}

void RemovePasspointCredentialsFailureCallback(
    const std::string& error_name,
    const std::string& error_message) {
  NET_LOG(ERROR) << "Failed to remove passpoint credentials, error:"
                 << error_name << ", message: " << error_message;
}

void SetLohsEnabledSuccessCallback(
    arc::ArcNetHostImpl::StartLohsCallback callback) {
  std::move(callback).Run(arc::mojom::LohsStatus::kSuccess);
}

void SetLohsEnabledFailureCallback(
    arc::ArcNetHostImpl::StartLohsCallback callback,
    const std::string& dbus_error_name,
    const std::string& dbus_error_message) {
  NET_LOG(ERROR) << "SetLohsEnabledFailureCallback, error: " << dbus_error_name
                 << ", message: " << dbus_error_message;
  // TODO(b/259162524): Change this to a more specific "shill configuration"
  // error
  std::move(callback).Run(arc::mojom::LohsStatus::kErrorGeneric);
}

void SetLohsConfigPropertySuccessCallback(
    arc::ArcNetHostImpl::StartLohsCallback callback) {
  auto callback_split = base::SplitOnceCallback(std::move(callback));
  ash::ShillManagerClient::Get()->SetLOHSEnabled(
      true /* enabled */,
      base::BindOnce(&SetLohsEnabledSuccessCallback,
                     std::move(callback_split.first)),
      base::BindOnce(&SetLohsEnabledFailureCallback,
                     std::move(callback_split.second)));
}

void SetLohsConfigPropertyFailureCallback(
    arc::ArcNetHostImpl::StartLohsCallback callback,
    const std::string& dbus_error_name,
    const std::string& dbus_error_message) {
  NET_LOG(ERROR) << "SetLohsConfigPropertyFailureCallback, error: "
                 << dbus_error_name << ", message: " << dbus_error_message;
  // TODO(b/259162524): Change this to a more specific "shill configuration"
  // error
  std::move(callback).Run(arc::mojom::LohsStatus::kErrorGeneric);
}

void StopLohsFailureCallback(const std::string& error_name,
                             const std::string& error_message) {
  NET_LOG(ERROR) << "StopLohsFailureCallback, error:" << error_name
                 << ", message: " << error_message;
}

}  // namespace

namespace arc {
namespace {

// Singleton factory for ArcNetHostImpl.
class ArcNetHostImplFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcNetHostImpl,
          ArcNetHostImplFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcNetHostImplFactory";

  static ArcNetHostImplFactory* GetInstance() {
    return base::Singleton<ArcNetHostImplFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcNetHostImplFactory>;
  ArcNetHostImplFactory() = default;
  ~ArcNetHostImplFactory() override = default;
};

}  // namespace

// static
ArcNetHostImpl* ArcNetHostImpl::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcNetHostImplFactory::GetForBrowserContext(context);
}

// static
ArcNetHostImpl* ArcNetHostImpl::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcNetHostImplFactory::GetForBrowserContextForTesting(context);
}

ArcNetHostImpl::ArcNetHostImpl(content::BrowserContext* context,
                               ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {
  arc_bridge_service_->net()->SetHost(this);
  arc_bridge_service_->net()->AddObserver(this);
}

ArcNetHostImpl::~ArcNetHostImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (observing_network_state_) {
    GetStateHandler()->RemoveObserver(this, FROM_HERE);
    GetNetworkConnectionHandler()->RemoveObserver(this);
  }
  arc_bridge_service_->net()->RemoveObserver(this);
  arc_bridge_service_->net()->SetHost(nullptr);
}

void ArcNetHostImpl::SetPrefService(PrefService* pref_service) {
  pref_service_ = pref_service;
}

void ArcNetHostImpl::SetCertManager(std::unique_ptr<CertManager> cert_manager) {
  cert_manager_ = std::move(cert_manager);
}

void ArcNetHostImpl::OnConnectionReady() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (ash::NetworkHandler::IsInitialized()) {
    GetStateHandler()->AddObserver(this, FROM_HERE);
    GetNetworkConnectionHandler()->AddObserver(this);
    observing_network_state_ = true;
  }

  // If the default network is an ARC VPN, that means Chrome is restarting
  // after a crash but shill still thinks a VPN is connected. Nuke it.
  const ash::NetworkState* default_network =
      GetShillBackedNetwork(GetStateHandler()->DefaultNetwork());
  if (default_network && default_network->type() == shill::kTypeVPN &&
      default_network->GetVpnProviderType() == shill::kProviderArcVpn) {
    GetNetworkConnectionHandler()->DisconnectNetwork(
        default_network->path(), base::BindOnce(&ArcVpnSuccessCallback),
        base::BindOnce(&ArcVpnErrorCallback, "disconnecting stale ARC VPN"));
  }

  // Listen on network configuration changes.
  ash::PatchPanelClient::Get()->AddObserver(this);

  SetUpFlags();
}

void ArcNetHostImpl::SetUpFlags() {
  auto* net_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->net(), SetUpFlag);
  if (!net_instance)
    return;

  net_instance->SetUpFlag(arc::mojom::Flag::ENABLE_ARC_HOST_VPN,
                          base::FeatureList::IsEnabled(arc::kEnableArcHostVpn));
}

void ArcNetHostImpl::OnConnectionClosed() {
  // Make sure shill doesn't leave an ARC VPN connected after Android
  // goes down.
  AndroidVpnStateChanged(arc::mojom::ConnectionStateType::NOT_CONNECTED);

  if (!observing_network_state_)
    return;

  GetStateHandler()->RemoveObserver(this, FROM_HERE);
  GetNetworkConnectionHandler()->RemoveObserver(this);
  observing_network_state_ = false;

  ash::PatchPanelClient::Get()->RemoveObserver(this);
}

void ArcNetHostImpl::NetworkConfigurationChanged() {
  // Get patchpanel devices and update active networks.
  ash::PatchPanelClient::Get()->GetDevices(base::BindOnce(
      &ArcNetHostImpl::UpdateActiveNetworks, weak_factory_.GetWeakPtr()));
}

void ArcNetHostImpl::GetNetworks(mojom::GetNetworksRequestType type,
                                 GetNetworksCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (type == mojom::GetNetworksRequestType::ACTIVE_ONLY) {
    ash::PatchPanelClient::Get()->GetDevices(
        base::BindOnce(&ArcNetHostImpl::GetActiveNetworks,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  // Otherwise retrieve list of configured or visible WiFi networks.
  bool configured_only = type == mojom::GetNetworksRequestType::CONFIGURED_ONLY;
  ash::NetworkTypePattern network_pattern =
      ash::onc::NetworkTypePatternFromOncType(onc::network_type::kWiFi);

  ash::NetworkStateHandler::NetworkStateList network_states;
  GetStateHandler()->GetNetworkListByType(
      network_pattern, configured_only, !configured_only /* visible_only */,
      kGetNetworksListLimit, &network_states);

  std::vector<mojom::NetworkConfigurationPtr> networks =
      TranslateNetworkStates(arc_vpn_service_path_, network_states,
                             shill_network_properties_, {} /* devices */);
  std::move(callback).Run(mojom::GetNetworksResponseType::New(
      arc::mojom::NetworkResult::SUCCESS, std::move(networks)));
}

void ArcNetHostImpl::GetActiveNetworks(
    GetNetworksCallback callback,
    const std::vector<patchpanel::NetworkDevice>& devices) {
  // Retrieve list of currently active networks.
  ash::NetworkStateHandler::NetworkStateList network_states;
  GetStateHandler()->GetActiveNetworkListByType(
      ash::NetworkTypePattern::Default(), &network_states);

  std::vector<mojom::NetworkConfigurationPtr> networks =
      TranslateNetworkStates(arc_vpn_service_path_, network_states,
                             shill_network_properties_, devices);
  std::move(callback).Run(mojom::GetNetworksResponseType::New(
      arc::mojom::NetworkResult::SUCCESS, std::move(networks)));
}

void ArcNetHostImpl::CreateNetworkSuccessCallback(
    base::OnceCallback<void(const std::string&)> callback,
    const std::string& service_path,
    const std::string& guid) {
  cached_guid_ = guid;
  cached_service_path_ = service_path;

  std::move(callback).Run(guid);
}

void ArcNetHostImpl::CreateNetworkFailureCallback(
    base::OnceCallback<void(const std::string&)> callback,
    const std::string& error_name) {
  NET_LOG(ERROR) << "CreateNetworkFailureCallback: " << error_name;
  std::move(callback).Run(std::string());
}

void ArcNetHostImpl::CreateNetwork(mojom::WifiConfigurationPtr cfg,
                                   CreateNetworkCallback callback) {
  if (!cfg->hexssid.has_value() || !cfg->details) {
    NET_LOG(ERROR)
        << "Cannot create WiFi network without hex ssid or WiFi properties";
    std::move(callback).Run(std::string());
    return;
  }

  mojom::ConfiguredNetworkDetailsPtr details =
      std::move(cfg->details->get_configured());
  if (!details) {
    NET_LOG(ERROR) << "Cannot create WiFi network without WiFi properties";
    std::move(callback).Run(std::string());
    return;
  }

  // TODO(b/195653632): Populate the shill EAP properties from the mojo
  // WifiConfiguration object.
  base::Value::Dict properties;
  base::Value::Dict wifi_dict;
  base::Value::Dict ipconfig_dict;

  properties.Set(onc::network_config::kType, onc::network_config::kWiFi);
  // StaticIPConfig dictionary
  wifi_dict.Set(onc::wifi::kHexSSID, cfg->hexssid.value());
  wifi_dict.Set(onc::wifi::kAutoConnect, details->autoconnect);
  if (cfg->security.empty()) {
    wifi_dict.Set(onc::wifi::kSecurity, onc::wifi::kSecurityNone);
  } else {
    wifi_dict.Set(onc::wifi::kSecurity, cfg->security);
    if (details->passphrase.has_value()) {
      wifi_dict.Set(onc::wifi::kPassphrase, details->passphrase.value());
    }
  }
  wifi_dict.Set(onc::wifi::kBSSID, cfg->bssid);
  properties.Set(onc::network_config::kWiFi, std::move(wifi_dict));

  // Set up static IPv4 config.
  if (cfg->dns_servers.has_value()) {
    ipconfig_dict.Set(onc::ipconfig::kNameServers,
                      TranslateStringListToValue(cfg->dns_servers.value()));
    properties.Set(onc::network_config::kNameServersConfigType,
                   onc::network_config::kIPConfigTypeStatic);
  }

  if (cfg->domains.has_value()) {
    ipconfig_dict.Set(onc::ipconfig::kSearchDomains,
                      TranslateStringListToValue(cfg->domains.value()));
  }

  // Static IPv4 address, static IPv4 address of the gateway and
  // prefix length are made sure to be all valid or all empty on
  // ARC side so we only need to check one of them.
  if (cfg->static_ipv4_config && cfg->static_ipv4_config->ipv4_addr) {
    ipconfig_dict.Set(onc::ipconfig::kType, onc::ipconfig::kIPv4);
    properties.Set(onc::network_config::kIPAddressConfigType,
                   onc::network_config::kIPConfigTypeStatic);
    ipconfig_dict.Set(onc::ipconfig::kIPAddress,
                      cfg->static_ipv4_config->ipv4_addr.value());
    ipconfig_dict.Set(onc::ipconfig::kGateway,
                      cfg->static_ipv4_config->gateway_ipv4_addr.value());
    ipconfig_dict.Set(onc::ipconfig::kRoutingPrefix,
                      cfg->static_ipv4_config->prefix_length);
  }
  if (cfg->http_proxy) {
    properties.Set(onc::network_config::kProxySettings,
                   TranslateProxyConfiguration(cfg->http_proxy));
  }

  // Set up meteredness based on meteredOverride config from mojom.
  if (cfg->metered_override == arc::mojom::MeteredOverride::kMetered) {
    properties.Set(onc::network_config::kMetered, true);
  } else if (cfg->metered_override ==
             arc::mojom::MeteredOverride::kNotmetered) {
    properties.Set(onc::network_config::kMetered, false);
  }

  if (!ipconfig_dict.empty()) {
    properties.Set(onc::network_config::kStaticIPConfig,
                   std::move(ipconfig_dict));
  }

  std::string user_id_hash = ash::LoginState::Get()->primary_user_hash();
  // TODO(crbug.com/730593): Remove SplitOnceCallback() by updating
  // the callee interface.
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  GetManagedConfigurationHandler()->CreateConfiguration(
      user_id_hash, base::Value(std::move(properties)),
      base::BindOnce(&ArcNetHostImpl::CreateNetworkSuccessCallback,
                     weak_factory_.GetWeakPtr(),
                     std::move(split_callback.first)),
      base::BindOnce(&ArcNetHostImpl::CreateNetworkFailureCallback,
                     weak_factory_.GetWeakPtr(),
                     std::move(split_callback.second)));
}

bool ArcNetHostImpl::GetNetworkPathFromGuid(const std::string& guid,
                                            std::string* path) {
  const auto* network =
      GetShillBackedNetwork(GetStateHandler()->GetNetworkStateFromGuid(guid));
  if (network) {
    *path = network->path();
    return true;
  }

  if (cached_guid_ == guid) {
    *path = cached_service_path_;
    return true;
  }

  return false;
}

void ArcNetHostImpl::ForgetNetwork(const std::string& guid,
                                   ForgetNetworkCallback callback) {
  std::string path;
  if (!GetNetworkPathFromGuid(guid, &path)) {
    NET_LOG(ERROR) << "Could not retrieve Service path from GUID " << guid;
    std::move(callback).Run(mojom::NetworkResult::FAILURE);
    return;
  }

  cached_guid_.clear();
  // TODO(crbug.com/730593): Remove SplitOnceCallback() by updating
  // the callee interface.
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  GetManagedConfigurationHandler()->RemoveConfigurationFromCurrentProfile(
      path,
      base::BindOnce(&ForgetNetworkSuccessCallback,
                     std::move(split_callback.first)),
      base::BindOnce(&ForgetNetworkFailureCallback,
                     std::move(split_callback.second)));
}

void ArcNetHostImpl::StartConnect(const std::string& guid,
                                  StartConnectCallback callback) {
  std::string path;
  if (!GetNetworkPathFromGuid(guid, &path)) {
    NET_LOG(ERROR) << "Could not retrieve Service path from GUID " << guid;
    std::move(callback).Run(mojom::NetworkResult::FAILURE);
    return;
  }

  // TODO(crbug.com/730593): Remove SplitOnceCallback() by updating
  // the callee interface.
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  GetNetworkConnectionHandler()->ConnectToNetwork(
      path,
      base::BindOnce(&StartConnectSuccessCallback,
                     std::move(split_callback.first)),
      base::BindOnce(&StartConnectFailureCallback,
                     std::move(split_callback.second)),
      false /* check_error_state */, ash::ConnectCallbackMode::ON_STARTED);
}

void ArcNetHostImpl::StartDisconnect(const std::string& guid,
                                     StartDisconnectCallback callback) {
  std::string path;
  if (!GetNetworkPathFromGuid(guid, &path)) {
    NET_LOG(ERROR) << "Could not retrieve Service path from GUID " << guid;
    std::move(callback).Run(mojom::NetworkResult::FAILURE);
    return;
  }

  // TODO(crbug.com/730593): Remove SplitOnceCallback() by updating
  // the callee interface.
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  GetNetworkConnectionHandler()->DisconnectNetwork(
      path,
      base::BindOnce(&StartDisconnectSuccessCallback,
                     std::move(split_callback.first)),
      base::BindOnce(&StartDisconnectFailureCallback,
                     std::move(split_callback.second)));
}

void ArcNetHostImpl::GetWifiEnabledState(GetWifiEnabledStateCallback callback) {
  bool is_enabled =
      GetStateHandler()->IsTechnologyEnabled(ash::NetworkTypePattern::WiFi());
  std::move(callback).Run(is_enabled);
}

void ArcNetHostImpl::SetWifiEnabledState(bool is_enabled,
                                         SetWifiEnabledStateCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto state =
      GetStateHandler()->GetTechnologyState(ash::NetworkTypePattern::WiFi());
  // WiFi can't be enabled or disabled in these states.
  if ((state == ash::NetworkStateHandler::TECHNOLOGY_PROHIBITED) ||
      (state == ash::NetworkStateHandler::TECHNOLOGY_UNINITIALIZED) ||
      (state == ash::NetworkStateHandler::TECHNOLOGY_UNAVAILABLE)) {
    NET_LOG(ERROR) << "SetWifiEnabledState failed due to WiFi state: " << state;
    std::move(callback).Run(false);
    return;
  }

  NET_LOG(USER) << __func__ << ":" << is_enabled;
  GetStateHandler()->SetTechnologyEnabled(
      ash::NetworkTypePattern::WiFi(), is_enabled,
      ash::network_handler::ErrorCallback());
  std::move(callback).Run(true);
}

void ArcNetHostImpl::StartScan() {
  GetStateHandler()->RequestScan(ash::NetworkTypePattern::WiFi());
}

void ArcNetHostImpl::ScanCompleted(const ash::DeviceState* /*unused*/) {
  auto* net_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->net(), ScanCompleted);
  if (!net_instance)
    return;

  net_instance->ScanCompleted();
}

void ArcNetHostImpl::DeviceListChanged() {
  auto* net_instance = ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->net(),
                                                   WifiEnabledStateChanged);
  if (!net_instance)
    return;

  bool is_enabled =
      GetStateHandler()->IsTechnologyEnabled(ash::NetworkTypePattern::WiFi());
  net_instance->WifiEnabledStateChanged(is_enabled);
}

std::string ArcNetHostImpl::LookupArcVpnServicePath() {
  ash::NetworkStateHandler::NetworkStateList state_list;
  GetStateHandler()->GetNetworkListByType(
      ash::NetworkTypePattern::VPN(), true /* configured_only */,
      false /* visible_only */, kGetNetworksListLimit, &state_list);

  for (const ash::NetworkState* state : state_list) {
    const auto* shill_backed_network = GetShillBackedNetwork(state);
    if (!shill_backed_network)
      continue;

    if (shill_backed_network->GetVpnProviderType() == shill::kProviderArcVpn)
      return shill_backed_network->path();
  }
  return std::string();
}

void ArcNetHostImpl::ConnectArcVpn(const std::string& service_path,
                                   const std::string& /* guid */) {
  arc_vpn_service_path_ = service_path;

  GetNetworkConnectionHandler()->ConnectToNetwork(
      service_path, base::BindOnce(&ArcVpnSuccessCallback),
      base::BindOnce(&ArcVpnErrorCallback, "connecting ARC VPN"),
      false /* check_error_state */, ash::ConnectCallbackMode::ON_COMPLETED);
}

base::Value::List ArcNetHostImpl::TranslateStringListToValue(
    const std::vector<std::string>& string_list) {
  base::Value::List result;
  for (const auto& item : string_list)
    result.Append(item);

  return result;
}

base::Value::List ArcNetHostImpl::TranslateLongListToStringValue(
    const std::vector<uint64_t>& long_list) {
  base::Value::List result;
  for (const auto& item : long_list)
    result.Append(base::NumberToString(item));

  return result;
}

base::Value::Dict ArcNetHostImpl::TranslateVpnConfigurationToOnc(
    const mojom::AndroidVpnConfiguration& cfg) {
  base::Value::Dict top_dict;

  // Name, Type
  top_dict.Set(onc::network_config::kName,
               cfg.session_name.empty() ? cfg.app_label : cfg.session_name);
  top_dict.Set(onc::network_config::kType, onc::network_config::kVPN);

  top_dict.Set(onc::network_config::kIPAddressConfigType,
               onc::network_config::kIPConfigTypeStatic);
  top_dict.Set(onc::network_config::kNameServersConfigType,
               onc::network_config::kIPConfigTypeStatic);

  base::Value::Dict ip_dict;
  ip_dict.Set(onc::ipconfig::kType, onc::ipconfig::kIPv4);
  ip_dict.Set(onc::ipconfig::kIPAddress, cfg.ipv4_gateway);
  ip_dict.Set(onc::ipconfig::kRoutingPrefix, 32);
  ip_dict.Set(onc::ipconfig::kGateway, cfg.ipv4_gateway);
  ip_dict.Set(onc::ipconfig::kNameServers,
              TranslateStringListToValue(cfg.nameservers));
  ip_dict.Set(onc::ipconfig::kSearchDomains,
              TranslateStringListToValue(cfg.domains));
  ip_dict.Set(onc::ipconfig::kIncludedRoutes,
              TranslateStringListToValue(cfg.split_include));
  ip_dict.Set(onc::ipconfig::kExcludedRoutes,
              TranslateStringListToValue(cfg.split_exclude));

  top_dict.Set(onc::network_config::kStaticIPConfig, std::move(ip_dict));

  // VPN dictionary
  base::Value::Dict vpn_dict;
  vpn_dict.Set(onc::vpn::kHost, cfg.app_name);
  vpn_dict.Set(onc::vpn::kType, onc::vpn::kArcVpn);

  // ARCVPN dictionary
  base::Value::Dict arcvpn_dict;
  arcvpn_dict.Set(onc::arc_vpn::kTunnelChrome,
                  cfg.tunnel_chrome_traffic ? "true" : "false");
  vpn_dict.Set(onc::vpn::kArcVpn, std::move(arcvpn_dict));

  top_dict.Set(onc::network_config::kVPN, std::move(vpn_dict));
  if (cfg.http_proxy) {
    top_dict.Set(onc::network_config::kProxySettings,
                 TranslateProxyConfiguration(cfg.http_proxy));
  }
  return top_dict;
}

void ArcNetHostImpl::AndroidVpnConnected(
    mojom::AndroidVpnConfigurationPtr cfg) {
  std::string service_path = LookupArcVpnServicePath();
  if (!service_path.empty()) {
    GetManagedConfigurationHandler()->SetProperties(
        service_path, base::Value(TranslateVpnConfigurationToOnc(*cfg)),
        base::BindOnce(&ArcNetHostImpl::ConnectArcVpn,
                       weak_factory_.GetWeakPtr(), service_path, std::string()),
        base::BindOnce(&ArcVpnErrorCallback,
                       "reconnecting ARC VPN " + service_path));
    return;
  }

  std::string user_id_hash = ash::LoginState::Get()->primary_user_hash();
  GetManagedConfigurationHandler()->CreateConfiguration(
      user_id_hash, base::Value(TranslateVpnConfigurationToOnc(*cfg)),
      base::BindOnce(&ArcNetHostImpl::ConnectArcVpn,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&ArcVpnErrorCallback, "connecting new ARC VPN"));
}

void ArcNetHostImpl::AndroidVpnStateChanged(mojom::ConnectionStateType state) {
  if (state != arc::mojom::ConnectionStateType::NOT_CONNECTED ||
      arc_vpn_service_path_.empty()) {
    return;
  }

  // DisconnectNetwork() invokes DisconnectRequested() through the
  // observer interface, so make sure it doesn't generate an unwanted
  // mojo call to Android.
  std::string service_path(arc_vpn_service_path_);
  arc_vpn_service_path_.clear();

  GetNetworkConnectionHandler()->DisconnectNetwork(
      service_path, base::BindOnce(&ArcVpnSuccessCallback),
      base::BindOnce(&ArcVpnErrorCallback, "disconnecting ARC VPN"));
}

void ArcNetHostImpl::TranslateEapCredentialsToDict(
    mojom::EapCredentialsPtr cred,
    base::OnceCallback<void(base::Value::Dict)> callback) {
  if (!cred) {
    NET_LOG(ERROR) << "Empty EAP credentials";
    return;
  }
  if (!cert_manager_) {
    NET_LOG(ERROR) << "CertManager is not initialized";
    return;
  }

  if (cred->client_certificate_key.has_value() &&
      cred->client_certificate_pem.has_value() &&
      cred->client_certificate_pem.value().size() > 0) {
    // |client_certificate_pem| contains all client certificates inside ARC's
    // PasspointConfiguration. ARC uses only one of the certificate that match
    // the certificate SHA-256 fingerprint. Currently, it is assumed that the
    // first certificate is the used certificate.
    // TODO(b/195262431): Remove the assumption by passing only the used
    // certificate to Chrome.
    // TODO(b/220803680): Remove imported certificates and keys when the
    // associated passpoint profile is removed.
    auto key = cred->client_certificate_key.value();
    auto pem = cred->client_certificate_pem.value()[0];
    cert_manager_->ImportPrivateKeyAndCert(
        key, pem,
        base::BindOnce(&ArcNetHostImpl::TranslateEapCredentialsToDictWithCertID,
                       weak_factory_.GetWeakPtr(), std::move(cred),
                       std::move(callback)));
    return;
  }
  TranslateEapCredentialsToDictWithCertID(std::move(cred), std::move(callback),
                                          /*cert_id=*/absl::nullopt,
                                          /*slot_id=*/absl::nullopt);
}

void ArcNetHostImpl::TranslateEapCredentialsToDictWithCertID(
    mojom::EapCredentialsPtr cred,
    base::OnceCallback<void(base::Value::Dict)> callback,
    const absl::optional<std::string>& cert_id,
    const absl::optional<int>& slot_id) {
  if (!cred) {
    NET_LOG(ERROR) << "Empty EAP credentials";
    return;
  }

  base::Value::Dict dict;
  dict.Set(shill::kEapMethodProperty, TranslateEapMethod(cred->method));
  dict.Set(shill::kEapPhase2AuthProperty,
           TranslateEapPhase2Method(cred->phase2_method));
  if (cred->anonymous_identity.has_value()) {
    dict.Set(shill::kEapAnonymousIdentityProperty,
             cred->anonymous_identity.value());
  }
  if (cred->identity.has_value())
    dict.Set(shill::kEapIdentityProperty, cred->identity.value());

  if (cred->password.has_value())
    dict.Set(shill::kEapPasswordProperty, cred->password.value());

  dict.Set(shill::kEapKeyMgmtProperty,
           TranslateKeyManagement(cred->key_management));

  if (cred->ca_certificate_pem.has_value()) {
    dict.Set(shill::kEapCaCertPemProperty,
             TranslateStringListToValue(cred->ca_certificate_pem.value()));
  }
  if (cert_id.has_value() && slot_id.has_value()) {
    // The ID of imported user certificate and private key is the same, use one
    // of them.
    dict.Set(
        shill::kEapKeyIdProperty,
        base::StringPrintf("%i:%s", slot_id.value(), cert_id.value().c_str()));
    dict.Set(
        shill::kEapCertIdProperty,
        base::StringPrintf("%i:%s", slot_id.value(), cert_id.value().c_str()));
    dict.Set(shill::kEapPinProperty, ash::client_cert::kDefaultTPMPin);
  }

  if (cred->subject_match.has_value()) {
    dict.Set(shill::kEapSubjectMatchProperty, cred->subject_match.value());
  }
  if (cred->subject_alternative_name_match_list.has_value()) {
    dict.Set(shill::kEapSubjectAlternativeNameMatchProperty,
             TranslateStringListToValue(
                 cred->subject_alternative_name_match_list.value()));
  }
  if (cred->domain_suffix_match_list.has_value()) {
    dict.Set(
        shill::kEapDomainSuffixMatchProperty,
        TranslateStringListToValue(cred->domain_suffix_match_list.value()));
  }
  if (cred->tls_version_max.has_value()) {
    dict.Set(shill::kEapTLSVersionMaxProperty, cred->tls_version_max.value());
  }
  dict.Set(shill::kEapUseSystemCasProperty, cred->use_system_cas);
  dict.Set(shill::kEapUseProactiveKeyCachingProperty,
           cred->use_proactive_key_caching);
  dict.Set(shill::kEapUseLoginPasswordProperty, cred->use_login_password);

  std::move(callback).Run(std::move(dict));
}

void ArcNetHostImpl::TranslatePasspointCredentialsToDict(
    mojom::PasspointCredentialsPtr cred,
    base::OnceCallback<void(base::Value::Dict)> callback) {
  if (!cred) {
    NET_LOG(ERROR) << "Empty passpoint credentials";
    return;
  }
  if (!cred->eap) {
    NET_LOG(ERROR) << "mojom::PasspointCredentials has no EAP properties";
    return;
  }

  mojom::EapCredentialsPtr eap = cred->eap.Clone();
  TranslateEapCredentialsToDict(
      std::move(eap),
      base::BindOnce(
          &ArcNetHostImpl::TranslatePasspointCredentialsToDictWithEapTranslated,
          weak_factory_.GetWeakPtr(), std::move(cred), std::move(callback)));
}

void ArcNetHostImpl::TranslatePasspointCredentialsToDictWithEapTranslated(
    mojom::PasspointCredentialsPtr cred,
    base::OnceCallback<void(base::Value::Dict)> callback,
    base::Value::Dict dict) {
  if (!cred) {
    NET_LOG(ERROR) << "Empty passpoint credentials";
    return;
  }
  if (dict.empty()) {
    NET_LOG(ERROR) << "Failed to translate EapCredentials properties";
    return;
  }

  dict.Set(shill::kPasspointCredentialsDomainsProperty,
           TranslateStringListToValue(cred->domains));
  dict.Set(shill::kPasspointCredentialsRealmProperty, cred->realm);
  dict.Set(shill::kPasspointCredentialsHomeOIsProperty,
           TranslateLongListToStringValue(cred->home_ois));
  dict.Set(shill::kPasspointCredentialsRequiredHomeOIsProperty,
           TranslateLongListToStringValue(cred->required_home_ois));
  dict.Set(shill::kPasspointCredentialsRoamingConsortiaProperty,
           TranslateLongListToStringValue(cred->roaming_consortium_ois));
  dict.Set(shill::kPasspointCredentialsMeteredOverrideProperty, cred->metered);
  dict.Set(shill::kPasspointCredentialsAndroidPackageNameProperty,
           cred->package_name);
  if (cred->friendly_name.has_value()) {
    dict.Set(shill::kPasspointCredentialsFriendlyNameProperty,
             cred->friendly_name.value());
  }
  dict.Set(shill::kPasspointCredentialsExpirationTimeMillisecondsProperty,
           base::NumberToString(cred->subscription_expiration_time_ms));

  std::move(callback).Run(std::move(dict));
}

// Set up proxy configuration. If proxy auto discovery pac url is available,
// we set up proxy auto discovery pac url, otherwise we set up
// host, port and exclusion list.
base::Value::Dict ArcNetHostImpl::TranslateProxyConfiguration(
    const arc::mojom::ArcProxyInfoPtr& http_proxy) {
  base::Value::Dict proxy_dict;
  if (http_proxy->is_pac_url_proxy()) {
    proxy_dict.Set(onc::proxy::kType, onc::proxy::kPAC);
    proxy_dict.Set(onc::proxy::kPAC,
                   http_proxy->get_pac_url_proxy()->pac_url.spec());
  } else {
    base::Value::Dict manual;
    manual.Set(onc::proxy::kHost, http_proxy->get_manual_proxy()->host);
    manual.Set(onc::proxy::kPort, http_proxy->get_manual_proxy()->port);
    manual.Set(onc::proxy::kExcludeDomains,
               TranslateStringListToValue(
                   std::move(http_proxy->get_manual_proxy()->exclusion_list)));
    proxy_dict.Set(onc::proxy::kType, onc::proxy::kManual);
    proxy_dict.Set(onc::proxy::kManual, std::move(manual));
  }
  return proxy_dict;
}

void ArcNetHostImpl::AddPasspointCredentials(
    mojom::PasspointCredentialsPtr credentials) {
  TranslatePasspointCredentialsToDict(
      std::move(credentials),
      base::BindOnce(&ArcNetHostImpl::AddPasspointCredentialsWithProperties,
                     weak_factory_.GetWeakPtr()));
}

void ArcNetHostImpl::AddPasspointCredentialsWithProperties(
    base::Value::Dict properties) {
  if (properties.empty()) {
    NET_LOG(ERROR) << "Failed to translate PasspointCredentials properties";
    return;
  }

  const auto* profile = GetNetworkProfile();
  if (!profile || profile->path.empty()) {
    NET_LOG(ERROR) << "Unable to get network profile path";
    return;
  }

  ash::ShillManagerClient::Get()->AddPasspointCredentials(
      dbus::ObjectPath(profile->path), base::Value(std::move(properties)),
      base::DoNothing(),
      base::BindOnce(&AddPasspointCredentialsFailureCallback));
  return;
}

void ArcNetHostImpl::RemovePasspointCredentials(
    mojom::PasspointRemovalPropertiesPtr properties) {
  if (!properties) {
    NET_LOG(ERROR) << "Empty passpoint removal properties";
    return;
  }

  const auto* profile = GetNetworkProfile();
  if (!profile || profile->path.empty()) {
    NET_LOG(ERROR) << "Unable to get network profile path";
    return;
  }

  base::Value::Dict shill_properties;
  if (properties->fqdn.has_value()) {
    shill_properties.Set(shill::kPasspointCredentialsFQDNProperty,
                         properties->fqdn.value());
  }
  if (properties->package_name.has_value()) {
    shill_properties.Set(shill::kPasspointCredentialsAndroidPackageNameProperty,
                         properties->package_name.value());
  }

  ash::ShillManagerClient::Get()->RemovePasspointCredentials(
      dbus::ObjectPath(profile->path), base::Value(std::move(shill_properties)),
      base::DoNothing(),
      base::BindOnce(&RemovePasspointCredentialsFailureCallback));

  return;
}

void ArcNetHostImpl::SetAlwaysOnVpn(const std::string& vpn_package,
                                    bool lockdown) {
  // pref_service_ should be set by ArcServiceLauncher.
  DCHECK(pref_service_);
  pref_service_->SetString(prefs::kAlwaysOnVpnPackage, vpn_package);
  pref_service_->SetBoolean(prefs::kAlwaysOnVpnLockdown, lockdown);
}

void ArcNetHostImpl::DisconnectHostVpn() {
  const ash::NetworkState* default_network =
      GetShillBackedNetwork(GetStateHandler()->DefaultNetwork());
  if (default_network && default_network->type() == shill::kTypeVPN &&
      default_network->GetVpnProviderType() != shill::kProviderArcVpn) {
    GetNetworkConnectionHandler()->DisconnectNetwork(
        default_network->path(), base::BindOnce(&HostVpnSuccessCallback),
        base::BindOnce(&HostVpnErrorCallback, "disconnecting host VPN"));
  }
}

void ArcNetHostImpl::DisconnectArcVpn() {
  arc_vpn_service_path_.clear();

  auto* net_instance = ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->net(),
                                                   DisconnectAndroidVpn);
  if (!net_instance)
    return;

  net_instance->DisconnectAndroidVpn();
}

void ArcNetHostImpl::DisconnectRequested(const std::string& service_path) {
  if (arc_vpn_service_path_ != service_path)
    return;

  // This code path is taken when a user clicks the blue Disconnect button
  // in Chrome OS.  Chrome is about to send the Disconnect call to shill,
  // so update our local state and tell Android to disconnect the VPN.
  DisconnectArcVpn();
}

void ArcNetHostImpl::NetworkConnectionStateChanged(
    const ash::NetworkState* network) {
  const auto* shill_backed_network = GetShillBackedNetwork(network);
  if (!shill_backed_network)
    return;

  if (arc_vpn_service_path_ != shill_backed_network->path() ||
      shill_backed_network->IsConnectingOrConnected()) {
    return;
  }

  // This code path is taken when shill disconnects the Android VPN
  // service.  This can happen if a user tries to connect to a Chrome OS
  // VPN, and shill's VPNProvider::DisconnectAll() forcibly disconnects
  // all other VPN services to avoid a conflict.
  DisconnectArcVpn();
}

void ArcNetHostImpl::NetworkPropertiesUpdated(
    const ash::NetworkState* network) {
  if (!IsActiveNetworkState(network))
    return;

  ash::NetworkHandler::Get()
      ->network_configuration_handler()
      ->GetShillProperties(
          network->path(),
          base::BindOnce(&ArcNetHostImpl::ReceiveShillProperties,
                         weak_factory_.GetWeakPtr()));
}

void ArcNetHostImpl::ReceiveShillProperties(
    const std::string& service_path,
    absl::optional<base::Value> shill_properties) {
  if (!shill_properties) {
    NET_LOG(ERROR) << "Failed to get shill Service properties for "
                   << service_path;
    return;
  }

  // Ignore properties received after the network has disconnected.
  const auto* network = GetStateHandler()->GetNetworkState(service_path);
  if (!IsActiveNetworkState(network))
    return;

  shill_network_properties_[service_path] = std::move(*shill_properties);

  // Get patchpanel devices and update active networks.
  ash::PatchPanelClient::Get()->GetDevices(base::BindOnce(
      &ArcNetHostImpl::UpdateActiveNetworks, weak_factory_.GetWeakPtr()));
}

void ArcNetHostImpl::UpdateActiveNetworks(
    const std::vector<patchpanel::NetworkDevice>& devices) {
  auto* net_instance = ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->net(),
                                                   ActiveNetworksChanged);
  if (!net_instance)
    return;

  net_instance->ActiveNetworksChanged(
      TranslateNetworkStates(arc_vpn_service_path_, GetHostActiveNetworks(),
                             shill_network_properties_, devices));
}

void ArcNetHostImpl::NetworkListChanged() {
  // Forget properties of disconnected networks
  base::EraseIf(shill_network_properties_, [](const auto& entry) {
    return !IsActiveNetworkState(
        GetStateHandler()->GetNetworkState(entry.first));
  });
  const auto active_networks = GetHostActiveNetworks();
  // If there is no active networks, send an explicit ActiveNetworksChanged
  // event to ARC and skip updating Shill properties.
  if (active_networks.empty()) {
    UpdateActiveNetworks({} /* devices */);
    return;
  }
  for (const auto* network : active_networks)
    NetworkPropertiesUpdated(network);
}

void ArcNetHostImpl::StartLohs(mojom::LohsConfigPtr config,
                               StartLohsCallback callback) {
  NET_LOG(USER) << "Starting LOHS";
  base::Value dict(base::Value::Type::DICTIONARY);

  if (config->hexssid.empty()) {
    NET_LOG(ERROR) << "Cannot create local only hotspot without hex ssid";
    std::move(callback).Run(arc::mojom::LohsStatus::kErrorGeneric);
    return;
  }
  dict.GetDict().Set(shill::kTetheringConfSSIDProperty,
                     base::Value(config->hexssid));

  if (config->band != arc::mojom::WifiBand::k2Ghz) {
    // TODO(b/257880335): Support 5Ghz band as well
    NET_LOG(ERROR) << "Unsupported band for LOHS: " << config->band
                   << "; can only support 2.4GHz";
    // TODO(b/259162524): Change this to a more specific "invalid argument"
    // error
    std::move(callback).Run(arc::mojom::LohsStatus::kErrorGeneric);
    return;
  }
  dict.GetDict().Set(shill::kTetheringConfBandProperty,
                     base::Value(shill::kBand2GHz));

  if (config->security_type != arc::mojom::SecurityType::WPA_PSK) {
    NET_LOG(ERROR) << "Unsupported security for LOHS: " << config->security_type
                   << "; can only support WPA_PSK";
    // TODO(b/259162524): Change this to a more specific "invalid argument"
    // error
    std::move(callback).Run(arc::mojom::LohsStatus::kErrorGeneric);
    return;
  }
  if (!config->passphrase.has_value()) {
    NET_LOG(ERROR) << "Cannot create local only hotspot without password";
    // TODO(b/259162524): Change this to a more specific "invalid argument"
    // error
    std::move(callback).Run(arc::mojom::LohsStatus::kErrorGeneric);
    return;
  }
  dict.GetDict().Set(shill::kTetheringConfSecurityProperty,
                     base::Value(shill::kSecurityWpa2));
  dict.GetDict().Set(shill::kTetheringConfPassphraseProperty,
                     base::Value(config->passphrase.value()));

  NET_LOG(USER) << "Set Shill Manager property: " << shill::kLOHSConfigProperty
                << ": " << dict;
  auto callback_split = base::SplitOnceCallback(std::move(callback));
  ash::ShillManagerClient::Get()->SetProperty(
      shill::kLOHSConfigProperty, dict,
      base::BindOnce(&SetLohsConfigPropertySuccessCallback,
                     std::move(callback_split.first)),
      base::BindOnce(&SetLohsConfigPropertyFailureCallback,
                     std::move(callback_split.second)));
}

void ArcNetHostImpl::StopLohs() {
  NET_LOG(USER) << "Stopping LOHS";
  ash::ShillManagerClient::Get()->SetLOHSEnabled(
      false /* enabled */, base::DoNothing(),
      base::BindOnce(&StopLohsFailureCallback));
}

void ArcNetHostImpl::OnShuttingDown() {
  DCHECK(observing_network_state_);
  GetStateHandler()->RemoveObserver(this, FROM_HERE);
  GetNetworkConnectionHandler()->RemoveObserver(this);
  observing_network_state_ = false;
}

}  // namespace arc
