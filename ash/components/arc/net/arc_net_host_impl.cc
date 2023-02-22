// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/net/arc_net_host_impl.h"

#include <net/if.h>

#include <utility>

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/net/arc_net_utils.h"
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
#include "chromeos/ash/components/network/technology_state_controller.h"
#include "components/device_event_log/device_event_log.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace {

constexpr int kGetNetworksListLimit = 100;

ash::NetworkStateHandler* GetStateHandler() {
  return ash::NetworkHandler::Get()->network_state_handler();
}

ash::TechnologyStateController* GetTechnologyStateController() {
  return ash::NetworkHandler::Get()->technology_state_controller();
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

void ForgetNetworkSuccessCallback(
    base::OnceCallback<void(arc::mojom::NetworkResult)> callback) {
  std::move(callback).Run(arc::mojom::NetworkResult::SUCCESS);
}

void ForgetNetworkFailureCallback(
    base::OnceCallback<void(arc::mojom::NetworkResult)> callback,
    const std::string& error_name) {
  std::move(callback).Run(arc::mojom::NetworkResult::FAILURE);
}

void UpdateWifiNetworkSuccessCallback(
    base::OnceCallback<void(arc::mojom::NetworkResult)> callback) {
  std::move(callback).Run(arc::mojom::NetworkResult::SUCCESS);
}

void UpdateWifiNetworkFailureCallback(
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

void HostVpnErrorCallback(const std::string& operation,
                          const std::string& error_name) {
  NET_LOG(ERROR) << "HostVpnErrorCallback: " << operation << ": " << error_name;
}

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
  std::move(callback).Run(arc::mojom::LohsStatus::kErrorConfiguringPlatform);
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
  std::move(callback).Run(arc::mojom::LohsStatus::kErrorConfiguringPlatform);
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
        default_network->path(),
        /*success_callback=*/base::DoNothing(),
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
      net_utils::TranslateNetworkStates(arc_vpn_service_path_, network_states,
                                        shill_network_properties_,
                                        {} /* devices */);
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
      net_utils::TranslateNetworkStates(arc_vpn_service_path_, network_states,
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
  if (cfg->bssid_allowlist.has_value()) {
    wifi_dict.Set(onc::wifi::kBSSIDAllowlist,
                  TranslateStringListToValue(cfg->bssid_allowlist.value()));
  }
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

void ArcNetHostImpl::UpdateWifiNetwork(const std::string& guid,
                                       mojom::WifiConfigurationPtr cfg,
                                       UpdateWifiNetworkCallback callback) {
  std::string path;
  if (!GetNetworkPathFromGuid(guid, &path)) {
    NET_LOG(ERROR) << "Could not retrieve Service path from GUID " << guid;
    std::move(callback).Run(mojom::NetworkResult::FAILURE);
    return;
  }

  // TODO(b/270089579): Add support for more properties to be updatable.
  base::Value::Dict properties;
  base::Value::Dict wifi_dict;

  if (cfg->bssid_allowlist.has_value()) {
    wifi_dict.Set(onc::wifi::kBSSIDAllowlist,
                  TranslateStringListToValue(cfg->bssid_allowlist.value()));
  }
  properties.Set(onc::network_config::kWiFi, std::move(wifi_dict));

  // TODO(crbug.com/730593): Remove SplitOnceCallback() by updating
  // the callee interface.
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  GetManagedConfigurationHandler()->SetProperties(
      path, base::Value(std::move(properties)),
      base::BindOnce(&UpdateWifiNetworkSuccessCallback,
                     std::move(split_callback.first)),
      base::BindOnce(&UpdateWifiNetworkFailureCallback,
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
  GetTechnologyStateController()->SetTechnologiesEnabled(
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
      service_path, /*success_callback=*/base::DoNothing(),
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
      service_path, /*success_callback=*/base::DoNothing(),
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
  dict.Set(shill::kEapMethodProperty,
           net_utils::TranslateEapMethod(cred->method));
  dict.Set(shill::kEapPhase2AuthProperty,
           net_utils::TranslateEapPhase2Method(cred->phase2_method));
  if (cred->anonymous_identity.has_value()) {
    dict.Set(shill::kEapAnonymousIdentityProperty,
             cred->anonymous_identity.value());
  }
  if (cred->identity.has_value())
    dict.Set(shill::kEapIdentityProperty, cred->identity.value());

  if (cred->password.has_value())
    dict.Set(shill::kEapPasswordProperty, cred->password.value());

  dict.Set(shill::kEapKeyMgmtProperty,
           net_utils::TranslateKeyManagement(cred->key_management));

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
      dbus::ObjectPath(profile->path), std::move(properties), base::DoNothing(),
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
      dbus::ObjectPath(profile->path), std::move(shill_properties),
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
        default_network->path(),
        /*success_callback=*/base::DoNothing(),
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
  if (!IsActiveNetworkState(network)) {
    return;
  }

  ash::NetworkHandler::Get()
      ->network_configuration_handler()
      ->GetShillProperties(
          network->path(),
          base::BindOnce(&ArcNetHostImpl::ReceiveShillProperties,
                         weak_factory_.GetWeakPtr()));
}

void ArcNetHostImpl::ReceiveShillProperties(
    const std::string& service_path,
    absl::optional<base::Value::Dict> shill_properties) {
  if (!shill_properties) {
    NET_LOG(ERROR) << "Failed to get shill Service properties for "
                   << service_path;
    return;
  }

  // Ignore properties received after the network has disconnected.
  const auto* network = GetStateHandler()->GetNetworkState(service_path);
  if (!IsActiveNetworkState(network)) {
    return;
  }

  shill_network_properties_[service_path] =
      base::Value(std::move(*shill_properties));

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

  net_instance->ActiveNetworksChanged(net_utils::TranslateNetworkStates(
      arc_vpn_service_path_, GetHostActiveNetworks(), shill_network_properties_,
      devices));
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
  base::Value dict(base::Value::Type::DICT);

  if (config->hexssid.empty()) {
    NET_LOG(ERROR) << "Cannot create local only hotspot without hex ssid";
    std::move(callback).Run(arc::mojom::LohsStatus::kErrorInvalidArgument);
    return;
  }
  dict.GetDict().Set(shill::kTetheringConfSSIDProperty,
                     base::Value(config->hexssid));

  if (config->band != arc::mojom::WifiBand::k2Ghz) {
    // TODO(b/257880335): Support 5Ghz band as well
    NET_LOG(ERROR) << "Unsupported band for LOHS: " << config->band
                   << "; can only support 2.4GHz";
    std::move(callback).Run(arc::mojom::LohsStatus::kErrorInvalidArgument);
    return;
  }
  dict.GetDict().Set(shill::kTetheringConfBandProperty,
                     base::Value(shill::kBand2GHz));

  if (config->security_type != arc::mojom::SecurityType::WPA_PSK) {
    NET_LOG(ERROR) << "Unsupported security for LOHS: " << config->security_type
                   << "; can only support WPA_PSK";
    std::move(callback).Run(arc::mojom::LohsStatus::kErrorInvalidArgument);
    return;
  }
  if (!config->passphrase.has_value()) {
    NET_LOG(ERROR) << "Cannot create local only hotspot without password";
    std::move(callback).Run(arc::mojom::LohsStatus::kErrorInvalidArgument);
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

// static
void ArcNetHostImpl::EnsureFactoryBuilt() {
  ArcNetHostImplFactory::GetInstance();
}

}  // namespace arc
