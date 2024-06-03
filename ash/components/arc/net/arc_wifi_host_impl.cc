// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/net/arc_wifi_host_impl.h"

#include <optional>

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/net/arc_net_utils.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "base/memory/singleton.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "chromeos/ash/components/network/onc/network_onc_utils.h"
#include "chromeos/ash/components/network/technology_state_controller.h"

namespace {
constexpr int kGetScanResultsListLimit = 100;

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

}  // namespace

namespace arc {
namespace {

// Singleton factory for ArcWifiHostImpl.
class ArcWifiHostImplFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcWifiHostImpl,
          ArcWifiHostImplFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcWifiHostImplFactory";

  static ArcWifiHostImplFactory* GetInstance() {
    return base::Singleton<ArcWifiHostImplFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcWifiHostImplFactory>;
  ArcWifiHostImplFactory() = default;
  ~ArcWifiHostImplFactory() override = default;
};

}  // namespace

// static
ArcWifiHostImpl* ArcWifiHostImpl::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcWifiHostImplFactory::GetForBrowserContext(context);
}

// static
ArcWifiHostImpl* ArcWifiHostImpl::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcWifiHostImplFactory::GetForBrowserContextForTesting(context);
}

ArcWifiHostImpl::ArcWifiHostImpl(content::BrowserContext* context,
                                 ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {
  arc_bridge_service_->arc_wifi()->SetHost(this);
  arc_bridge_service_->arc_wifi()->AddObserver(this);
}

ArcWifiHostImpl::~ArcWifiHostImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  arc_bridge_service_->arc_wifi()->RemoveObserver(this);
  arc_bridge_service_->arc_wifi()->SetHost(nullptr);
}

// static
void ArcWifiHostImpl::EnsureFactoryBuilt() {
  ArcWifiHostImplFactory::GetInstance();
}

void ArcWifiHostImpl::GetWifiEnabledState(
    GetWifiEnabledStateCallback callback) {
  bool is_enabled =
      GetStateHandler()->IsTechnologyEnabled(ash::NetworkTypePattern::WiFi());
  std::move(callback).Run(is_enabled);
}

void ArcWifiHostImpl::SetWifiEnabledState(
    bool is_enabled,
    SetWifiEnabledStateCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto state =
      GetStateHandler()->GetTechnologyState(ash::NetworkTypePattern::WiFi());
  // WiFi can't be enabled or disabled in these states.
  switch (state) {
    case ash::NetworkStateHandler::TECHNOLOGY_PROHIBITED:
    case ash::NetworkStateHandler::TECHNOLOGY_UNINITIALIZED:
    case ash::NetworkStateHandler::TECHNOLOGY_UNAVAILABLE:
      // If WiFi is in above state, it is already disabled. This is a noop.
      if (!is_enabled) {
        std::move(callback).Run(true);
        return;
      }
      NET_LOG(ERROR) << __func__ << ": failed due to WiFi state: " << state;
      std::move(callback).Run(false);
      return;
    default:
      break;
  }

  NET_LOG(USER) << __func__ << ": " << is_enabled;
  GetTechnologyStateController()->SetTechnologiesEnabled(
      ash::NetworkTypePattern::WiFi(), is_enabled,
      ash::network_handler::ErrorCallback());
  std::move(callback).Run(true);
}

void ArcWifiHostImpl::CreateNetwork(
    mojom::WifiConfigurationPtr cfg,
    ArcNetHostImpl::CreateNetworkCallback callback) {
  if (!cfg->eap) {
    CreateNetworkWithEapTranslated(std::move(cfg), std::move(callback), {});
    return;
  }
  mojom::EapCredentialsPtr eap = cfg->eap.Clone();
  TranslateEapCredentialsToDict(
      std::move(eap),
      /*is_onc=*/true,
      base::BindOnce(&ArcWifiHostImpl::CreateNetworkWithEapTranslated,
                     weak_factory_.GetWeakPtr(), std::move(cfg),
                     std::move(callback)));
}

void ArcWifiHostImpl::CreateNetworkWithEapTranslated(
    mojom::WifiConfigurationPtr cfg,
    ArcNetHostImpl::CreateNetworkCallback callback,
    base::Value::Dict eap_dict) {
  if (!cfg->hexssid.has_value() || !cfg->details) {
    NET_LOG(ERROR) << __func__ << ": Cannot create WiFi network without hex"
                   << " SSID or WiFi properties";
    std::move(callback).Run(std::string());
    return;
  }
  // TODO(b/329552433): Deprecate NetworkDetails after
  // ArcNetHostImpl::CreateNetwork is removed.
  mojom::ConfiguredNetworkDetailsPtr details =
      std::move(cfg->details->get_configured());
  if (!details) {
    NET_LOG(ERROR) << __func__
                   << ": Cannot create WiFi network without WiFi properties";
    std::move(callback).Run(std::string());
    return;
  }

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
  if (details->bssid.has_value()) {
    wifi_dict.Set(onc::wifi::kBSSIDRequested, details->bssid.value());
  }
  if (cfg->bssid_allowlist.has_value()) {
    wifi_dict.Set(
        onc::wifi::kBSSIDAllowlist,
        net_utils::TranslateStringListToValue(cfg->bssid_allowlist.value()));
  }
  if (!eap_dict.empty()) {
    wifi_dict.Set(onc::wifi::kEAP, std::move(eap_dict));
  }

  properties.Set(onc::network_config::kWiFi, std::move(wifi_dict));

  // Set up static IPv4 config.
  if (cfg->dns_servers.has_value()) {
    ipconfig_dict.Set(
        onc::ipconfig::kNameServers,
        net_utils::TranslateStringListToValue(cfg->dns_servers.value()));
    properties.Set(onc::network_config::kNameServersConfigType,
                   onc::network_config::kIPConfigTypeStatic);
  }

  if (cfg->domains.has_value()) {
    ipconfig_dict.Set(
        onc::ipconfig::kSearchDomains,
        net_utils::TranslateStringListToValue(cfg->domains.value()));
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
                   net_utils::TranslateProxyConfiguration(*(cfg->http_proxy)));
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

  auto split_callback = base::SplitOnceCallback(std::move(callback));
  GetManagedConfigurationHandler()->CreateConfiguration(
      user_id_hash, properties,
      base::BindOnce(&ArcWifiHostImpl::CreateNetworkSuccessCallback,
                     weak_factory_.GetWeakPtr(),
                     std::move(split_callback.first)),
      base::BindOnce(&ArcWifiHostImpl::CreateNetworkFailureCallback,
                     weak_factory_.GetWeakPtr(),
                     std::move(split_callback.second)));
}

void ArcWifiHostImpl::TranslateEapCredentialsToDict(
    mojom::EapCredentialsPtr cred,
    bool is_onc,
    base::OnceCallback<void(base::Value::Dict)> callback) {
  if (!cred) {
    NET_LOG(ERROR) << __func__ << ": Empty EAP credentials";
    return;
  }
  if (!cert_manager_) {
    NET_LOG(ERROR) << __func__ << ": CertManager is not initialized";
    return;
  }

  bool has_client_cert = cred->client_certificate_key.has_value() &&
                         cred->client_certificate_pem.has_value() &&
                         !cred->client_certificate_pem.value().empty();

  std::string key;
  std::string pem;
  if (has_client_cert) {
    key = cred->client_certificate_key.value();
    pem = cred->client_certificate_pem.value()[0];
  }

  CertManager::ImportPrivateKeyAndCertCallback continue_callback =
      base::BindOnce(
          is_onc
              ? &ArcWifiHostImpl::TranslateEapCredentialsToOncDictWithCertID
              : &ArcWifiHostImpl::TranslateEapCredentialsToShillDictWithCertID,
          weak_factory_.GetWeakPtr(), std::move(cred), std::move(callback));

  if (has_client_cert) {
    // |client_certificate_pem| contains all client certificates inside ARC's
    // PasspointConfiguration. ARC uses only one of the certificate that match
    // the certificate SHA-256 fingerprint. Currently, it is assumed that the
    // first certificate is the used certificate.
    // TODO(b/195262431): Remove the assumption by passing only the used
    // certificate to Chrome.
    // TODO(b/220803680): Remove imported certificates and keys when the
    // associated passpoint profile is removed.
    cert_manager_->ImportPrivateKeyAndCert(key, pem,
                                           std::move(continue_callback));
    return;
  }
  std::move(continue_callback)
      .Run(/*cert_id=*/std::nullopt,
           /*slot_id=*/std::nullopt);
}

void ArcWifiHostImpl::TranslateEapCredentialsToOncDictWithCertID(
    const mojom::EapCredentialsPtr& eap,
    base::OnceCallback<void(base::Value::Dict)> callback,
    const std::optional<std::string>& cert_id,
    const std::optional<int>& slot_id) {
  base::Value::Dict eap_dict;

  if (cert_id.has_value() && slot_id.has_value()) {
    // The ID of imported user certificate and private key is the same, use one
    // of them.
    eap_dict.Set(
        onc::client_cert::kClientCertPKCS11Id,
        base::StringPrintf("%i:%s", slot_id.value(), cert_id.value().c_str()));
  }
  eap_dict.Set(onc::client_cert::kClientCertType, onc::client_cert::kPKCS11Id);
  eap_dict.Set(onc::eap::kOuter,
               net_utils::TranslateEapMethodToOnc(eap->method));
  if (!net_utils::TranslateEapPhase2MethodToOnc(eap->phase2_method).empty()) {
    eap_dict.Set(onc::eap::kInner,
                 net_utils::TranslateEapPhase2MethodToOnc(eap->phase2_method));
  }
  if (eap->identity.has_value()) {
    eap_dict.Set(onc::eap::kIdentity, eap->identity.value());
  }
  if (eap->password.has_value()) {
    eap_dict.Set(onc::eap::kPassword, eap->password.value());
  }
  if (eap->anonymous_identity.has_value()) {
    eap_dict.Set(onc::eap::kAnonymousIdentity, eap->anonymous_identity.value());
  }
  if (eap->tls_version_max.has_value()) {
    eap_dict.Set(onc::eap::kTLSVersionMax, eap->tls_version_max.value());
  }
  eap_dict.Set(onc::eap::kUseSystemCAs, eap->use_system_cas);
  if (eap->subject_match.has_value()) {
    eap_dict.Set(onc::eap::kSubjectMatch, eap->subject_match.value());
  }
  if (eap->subject_alternative_name_match_list.has_value()) {
    eap_dict.Set(onc::eap::kSubjectAlternativeNameMatch,
                 net_utils::TranslateSubjectNameMatchListToValue(
                     eap->subject_alternative_name_match_list.value()));
  }
  if (eap->ca_certificate_pem.has_value()) {
    eap_dict.Set(onc::eap::kServerCAPEMs, net_utils::TranslateStringListToValue(
                                              eap->ca_certificate_pem.value()));
  }
  eap_dict.Set(onc::wifi::kSecurity,
               net_utils::TranslateKeyManagementToOnc(eap->key_management));
  if (eap->domain_suffix_match_list.has_value()) {
    eap_dict.Set(onc::eap::kDomainSuffixMatch,
                 net_utils::TranslateStringListToValue(
                     eap->domain_suffix_match_list.value()));
  }
  // Field eap->use_login_password is ignored for now, as using user's password
  // by a third part app is not allowed at the moment.

  std::move(callback).Run(std::move(eap_dict));
}

void ArcWifiHostImpl::TranslateEapCredentialsToShillDictWithCertID(
    const mojom::EapCredentialsPtr& cred,
    base::OnceCallback<void(base::Value::Dict)> callback,
    const std::optional<std::string>& cert_id,
    const std::optional<int>& slot_id) {
  if (!cred) {
    NET_LOG(ERROR) << __func__ << ": Empty EAP credentials";
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
  if (cred->identity.has_value()) {
    dict.Set(shill::kEapIdentityProperty, cred->identity.value());
  }

  if (cred->password.has_value()) {
    dict.Set(shill::kEapPasswordProperty, cred->password.value());
  }

  dict.Set(shill::kEapKeyMgmtProperty,
           net_utils::TranslateKeyManagement(cred->key_management));

  if (cred->ca_certificate_pem.has_value()) {
    dict.Set(shill::kEapCaCertPemProperty,
             net_utils::TranslateStringListToValue(
                 cred->ca_certificate_pem.value()));
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
             net_utils::TranslateStringListToValue(
                 cred->subject_alternative_name_match_list.value()));
  }
  if (cred->domain_suffix_match_list.has_value()) {
    dict.Set(shill::kEapDomainSuffixMatchProperty,
             net_utils::TranslateStringListToValue(
                 cred->domain_suffix_match_list.value()));
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

void ArcWifiHostImpl::CreateNetworkSuccessCallback(
    base::OnceCallback<void(const std::string&)> callback,
    const std::string& service_path,
    const std::string& guid) {
  cached_guid_ = guid;
  cached_service_path_ = service_path;

  std::move(callback).Run(guid);
}

void ArcWifiHostImpl::CreateNetworkFailureCallback(
    base::OnceCallback<void(const std::string&)> callback,
    const std::string& error_name) {
  NET_LOG(ERROR) << __func__ << ": " << error_name;
  std::move(callback).Run(std::string());
}

void ArcWifiHostImpl::ForgetNetwork(const std::string& guid,
                                    ForgetNetworkCallback callback) {
  auto path = GetNetworkPathFromGuid(guid);
  if (!path) {
    NET_LOG(ERROR) << __func__ << ": Could not retrieve Service path from GUID "
                   << guid;
    std::move(callback).Run(mojom::NetworkResult::FAILURE);
    return;
  }

  cached_guid_.clear();
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  GetManagedConfigurationHandler()->RemoveConfigurationFromCurrentProfile(
      std::string(*path),
      base::BindOnce(&ForgetNetworkSuccessCallback,
                     std::move(split_callback.first)),
      base::BindOnce(&ForgetNetworkFailureCallback,
                     std::move(split_callback.second)));
}

std::optional<std::string_view> ArcWifiHostImpl::GetNetworkPathFromGuid(
    const std::string& guid) {
  const auto* network =
      GetShillBackedNetwork(GetStateHandler()->GetNetworkStateFromGuid(guid));
  if (network) {
    return network->path();
  }

  if (cached_guid_ == guid) {
    return cached_service_path_;
  }

  return std::nullopt;
}

void ArcWifiHostImpl::UpdateWifiNetwork(const std::string& guid,
                                        mojom::WifiConfigurationPtr cfg,
                                        UpdateWifiNetworkCallback callback) {
  auto path = GetNetworkPathFromGuid(guid);
  if (!path) {
    NET_LOG(ERROR) << __func__ << ": Could not retrieve Service path from GUID "
                   << guid;
    std::move(callback).Run(mojom::NetworkResult::FAILURE);
    return;
  }

  // TODO(b/270089579): Add support for more properties to be updatable.
  base::Value::Dict properties;
  base::Value::Dict wifi_dict;

  if (cfg->bssid_allowlist.has_value()) {
    wifi_dict.Set(
        onc::wifi::kBSSIDAllowlist,
        net_utils::TranslateStringListToValue(cfg->bssid_allowlist.value()));
  }
  properties.Set(onc::network_config::kWiFi, std::move(wifi_dict));

  auto split_callback = base::SplitOnceCallback(std::move(callback));
  GetManagedConfigurationHandler()->SetProperties(
      std::string(*path), properties,
      base::BindOnce(&UpdateWifiNetworkSuccessCallback,
                     std::move(split_callback.first)),
      base::BindOnce(&UpdateWifiNetworkFailureCallback,
                     std::move(split_callback.second)));
}

void ArcWifiHostImpl::GetConfiguredWifiServices(
    GetConfiguredWifiServicesCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  ash::NetworkTypePattern network_pattern =
      ash::onc::NetworkTypePatternFromOncType(onc::network_type::kWiFi);

  ash::NetworkStateHandler::NetworkStateList network_states;
  GetStateHandler()->GetNetworkListByType(
      network_pattern, /*configured_only=*/true, /*visible_only=*/false,
      kGetNetworksListLimit, &network_states);

  std::move(callback).Run(net_utils::TranslateNetworkStates(
      /*arc_vpn_service_path=*/"", network_states));
}

void ArcWifiHostImpl::SetCertManager(
    std::unique_ptr<CertManager> cert_manager) {
  cert_manager_ = std::move(cert_manager);
}

void ArcWifiHostImpl::StartScan() {
  GetStateHandler()->RequestScan(ash::NetworkTypePattern::WiFi());
}

void ArcWifiHostImpl::GetScanResults(GetScanResultsCallback callback) {
  ash::NetworkTypePattern network_pattern =
      ash::onc::NetworkTypePatternFromOncType(onc::network_type::kWiFi);

  ash::NetworkStateHandler::NetworkStateList network_states;
  GetStateHandler()->GetNetworkListByType(
      network_pattern, /*configured_only=*/false, /*visible_only=*/true,
      kGetScanResultsListLimit, &network_states);

  std::move(callback).Run(net_utils::TranslateScanResults(network_states));
}

}  // namespace arc
