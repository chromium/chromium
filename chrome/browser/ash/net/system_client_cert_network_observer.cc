// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/system_client_cert_network_observer.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/ash/kcer/kcer_factory_ash.h"
#include "chrome/browser/ash/net/client_cert_store_kcer.h"
#include "chromeos/ash/components/kcer/extra_instances.h"
#include "chromeos/components/certificate_provider/certificate_provider.h"
#include "chromeos/components/onc/certificate_scope.h"
#include "components/certificate_matching/auto_select_certificates.h"
#include "components/content_settings/core/browser/content_settings_policy_provider.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/session_manager/core/session.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_type.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/client_cert_store_nss.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_private_key.h"
#include "third_party/boringssl/src/include/openssl/pool.h"
#include "url/gurl.h"

namespace ash {

namespace {

const base::ListValue* GetAutoSelectCertificateRules(
    const policy::PolicyService& device_policy_service) {
  const base::Value* value =
      device_policy_service
          .GetPolicies(policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME,
                                               std::string()))
          .GetValue(policy::key::kDeviceLoginScreenAutoSelectCertificateForUrls,
                    base::Value::Type::LIST);
  return value ? &value->GetList() : nullptr;
}

// Counterpart to
// `ProfileNetworkContextService::CreateClientCertIssuerSourcesWithDBCerts` in
// `chrome/browser/net/profile_network_context_service.cc`, scoped to
// device-wide ONC authority certificates and NSS intermediates.
void CreateDeviceClientCertIssuerSources(
    net::CertificateList device_onc_certs,
    net::ClientCertIssuerSourceGetterCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> certs;
  certs.reserve(device_onc_certs.size());
  for (const auto& cert : device_onc_certs) {
    certs.push_back(bssl::UpRef(cert->cert_buffer()));
  }

  net::ClientCertIssuerSourceCollection sources;
  if (!certs.empty()) {
    sources.push_back(std::make_unique<net::ClientCertIssuerSourceInMemory>(
        std::move(certs)));
  }
  sources.push_back(
      std::make_unique<net::ClientCertStoreNSS::IssuerSourceNSS>());
  std::move(callback).Run(std::move(sources));
}

std::unique_ptr<net::ClientCertStore> CreateDefaultClientCertStore(
    net::CertificateList device_onc_certs) {
  // Ensure KcerFactoryAsh has scheduled DeviceKcer initialization.
  kcer::KcerFactoryAsh::GetInstance();
  return std::make_unique<ClientCertStoreKcer>(
      /*cert_provider=*/nullptr, kcer::ExtraInstances::GetDeviceKcer(),
      base::BindOnce(&CreateDeviceClientCertIssuerSources,
                     std::move(device_onc_certs)));
}

// Adapts `net::SSLPrivateKey` to `network::mojom::SSLPrivateKey`. Mirrors
// `content::SSLPrivateKeyImpl` (`content/browser/ssl_private_key_impl.cc`),
// which is internal to `//content/browser`.
// TODO(b/586891345): Centralize `SSLPrivateKeyImpl` so it can be shared
// outside `//content/browser`.
class SSLPrivateKeyWrapper : public network::mojom::SSLPrivateKey {
 public:
  explicit SSLPrivateKeyWrapper(
      scoped_refptr<net::SSLPrivateKey> ssl_private_key)
      : ssl_private_key_(std::move(ssl_private_key)) {}

  SSLPrivateKeyWrapper(const SSLPrivateKeyWrapper&) = delete;
  SSLPrivateKeyWrapper& operator=(const SSLPrivateKeyWrapper&) = delete;

  ~SSLPrivateKeyWrapper() override = default;

  // network::mojom::SSLPrivateKey:
  void Sign(uint16_t algorithm,
            const std::vector<uint8_t>& input,
            network::mojom::SSLPrivateKey::SignCallback callback) override {
    if (!std::ranges::contains(ssl_private_key_->GetAlgorithmPreferences(),
                               algorithm)) {
      std::move(callback).Run(
          int32_t{net::ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED},
          std::vector<uint8_t>());
      return;
    }
    // In some cases, `ssl_private_key_` may abandon the callback. See
    // https://crbug.com/40651689.
    callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
        std::move(callback), int32_t{net::ERR_FAILED}, std::vector<uint8_t>());
    ssl_private_key_->Sign(
        algorithm, input,
        base::BindOnce(&SSLPrivateKeyWrapper::OnSigned, std::move(callback)));
  }

 private:
  static void OnSigned(network::mojom::SSLPrivateKey::SignCallback callback,
                       net::Error net_error,
                       const std::vector<uint8_t>& signature) {
    std::move(callback).Run(static_cast<int32_t>(net_error), signature);
  }

  scoped_refptr<net::SSLPrivateKey> ssl_private_key_;
};

}  // namespace

// static
SystemClientCertNetworkObserver::ClientCertStoreFactory
SystemClientCertNetworkObserver::CreateDefaultClientCertStoreFactory() {
  return base::BindRepeating(&CreateDefaultClientCertStore);
}

SystemClientCertNetworkObserver::SystemClientCertNetworkObserver(
    session_manager::SessionManager* session_manager,
    user_manager::UserManager* user_manager,
    policy::PolicyService* device_policy_service,
    PolicyCertificateProvider* device_policy_cert_provider,
    base::RepeatingClosure flush_client_cert_cache_callback,
    DataUseCallback data_use_callback,
    ClientCertStoreFactory client_cert_store_factory)
    : session_manager_(CHECK_DEREF(session_manager)),
      user_manager_(CHECK_DEREF(user_manager)),
      flush_client_cert_cache_callback_(
          std::move(flush_client_cert_cache_callback)),
      data_use_callback_(std::move(data_use_callback)),
      client_cert_store_factory_(std::move(client_cert_store_factory)),
      was_allowed_by_session_state_(IsAllowedBySessionState()) {
  CHECK(device_policy_service);
  CHECK(device_policy_cert_provider);
  CHECK(flush_client_cert_cache_callback_);
  CHECK(data_use_callback_);
  CHECK(client_cert_store_factory_);

  policy_change_registrar_ = std::make_unique<policy::PolicyChangeRegistrar>(
      device_policy_service,
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string()));
  policy_change_registrar_->Observe(
      policy::key::kDeviceLoginScreenAutoSelectCertificateForUrls,
      base::BindRepeating(&SystemClientCertNetworkObserver::
                              OnAutoSelectCertificatePatternsChanged,
                          base::Unretained(this)));
  if (const base::ListValue* rules =
          GetAutoSelectCertificateRules(*device_policy_service)) {
    base::AutoLock auto_lock(policy_value_map_.GetLock());
    content_settings::PolicyProvider::PopulateAutoSelectCertificateSettings(
        *rules, policy_value_map_);
  }

  user_manager_observation_.Observe(&user_manager_.get());
  device_policy_cert_provider_observation_.Observe(device_policy_cert_provider);
}

SystemClientCertNetworkObserver::~SystemClientCertNetworkObserver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void SystemClientCertNetworkObserver::BindReceiver(
    mojo::PendingReceiver<network::mojom::URLLoaderNetworkServiceObserver>
        receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  receivers_.Add(this, std::move(receiver));
}

void SystemClientCertNetworkObserver::OnSSLCertificateError(
    const GURL& url,
    int net_error,
    const net::SSLInfo& ssl_info,
    bool fatal,
    OnSSLCertificateErrorCallback response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(response).Run(net_error);
}

void SystemClientCertNetworkObserver::OnCertificateRequested(
    const std::optional<base::UnguessableToken>& window_id,
    const scoped_refptr<net::SSLCertRequestInfo>& cert_info,
    mojo::PendingRemote<network::mojom::ClientCertificateResponder>
        cert_responder) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsAllowedBySessionState()) {
    mojo::Remote<network::mojom::ClientCertificateResponder>(
        std::move(cert_responder))
        ->CancelRequest();
    return;
  }

  GURL requesting_url("https://" + cert_info->host_and_port.ToString());
  if (!requesting_url.is_valid()) {
    mojo::Remote<network::mojom::ClientCertificateResponder>(
        std::move(cert_responder))
        ->CancelRequest();
    return;
  }

  base::ListValue filters = GetAutoSelectionFilters(requesting_url);
  if (filters.empty()) {
    mojo::Remote<network::mojom::ClientCertificateResponder>(
        std::move(cert_responder))
        ->CancelRequest();
    return;
  }

  const uint64_t request_generation = state_generation_;
  PolicyCertificateProvider* provider =
      device_policy_cert_provider_observation_.GetSource();
  net::CertificateList device_onc_certs =
      provider ? provider->GetAllServerAndAuthorityCertificates(
                     chromeos::onc::CertificateScope::Default())
               : net::CertificateList();
  std::unique_ptr<net::ClientCertStore> cert_store =
      client_cert_store_factory_.Run(std::move(device_onc_certs));
  net::ClientCertStore* raw_cert_store = cert_store.get();
  raw_cert_store->GetClientCerts(
      cert_info,
      base::BindOnce(&SystemClientCertNetworkObserver::OnClientCertsRetrieved,
                     weak_factory_.GetWeakPtr(), request_generation,
                     std::move(filters), std::move(cert_responder),
                     std::move(cert_store)));
}

void SystemClientCertNetworkObserver::OnAuthRequired(
    const std::optional<base::UnguessableToken>& window_id,
    int32_t request_id,
    const GURL& url,
    bool first_auth_attempt,
    bool do_not_prompt_for_login,
    const net::AuthChallengeInfo& auth_info,
    const scoped_refptr<net::HttpResponseHeaders>& head_headers,
    mojo::PendingRemote<network::mojom::AuthChallengeResponder>
        auth_challenge_responder) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  mojo::Remote<network::mojom::AuthChallengeResponder>(
      std::move(auth_challenge_responder))
      ->OnAuthCredentials(std::nullopt);
}

void SystemClientCertNetworkObserver::OnLocalNetworkAccessPermissionRequired(
    network::mojom::TransportType type,
    network::mojom::IPAddressSpace ip_address_space,
    OnLocalNetworkAccessPermissionRequiredCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(network::mojom::LocalNetworkAccessResult::kDenied);
}

void SystemClientCertNetworkObserver::OnPlatformLocalNetworkPermissionRequired(
    OnPlatformLocalNetworkPermissionRequiredCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(/*granted=*/false);
}

void SystemClientCertNetworkObserver::OnClearSiteData(
    const GURL& url,
    const std::optional<net::SchemefulSite>& top_level_site,
    const std::optional<base::UnguessableToken>& nonce,
    const std::string& header_value,
    int load_flags,
    const std::optional<net::CookiePartitionKey>& cookie_partition_key,
    bool partitioned_state_allowed_only,
    OnClearSiteDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run();
}

void SystemClientCertNetworkObserver::OnLoadingStateUpdate(
    network::mojom::LoadInfoPtr info,
    OnLoadingStateUpdateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run();
}

void SystemClientCertNetworkObserver::OnDataUseUpdate(
    int32_t network_traffic_annotation_id_hash,
    base::ByteSize recv_bytes,
    base::ByteSize sent_bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(b/586891897): Deduplicate default `URLLoaderNetworkServiceObserver`
  // methods via a shared base class.
  data_use_callback_.Run(network_traffic_annotation_id_hash, recv_bytes,
                         sent_bytes);
}

void SystemClientCertNetworkObserver::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderNetworkServiceObserver>
        listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  receivers_.Add(this, std::move(listener));
}

void SystemClientCertNetworkObserver::OnWebSocketConnectedToLocalNetwork(
    const GURL& request_url,
    network::mojom::IPAddressSpace ip_address_space) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void SystemClientCertNetworkObserver::OnUrlLoaderConnectedToLocalNetwork(
    const GURL& request_url,
    network::mojom::IPAddressSpace response_address_space,
    network::mojom::IPAddressSpace client_address_space,
    network::mojom::IPAddressSpace target_address_space) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void SystemClientCertNetworkObserver::OnUserLoggedIn(
    const user_manager::User& user) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UpdateSessionAllowedState();
}

void SystemClientCertNetworkObserver::OnUserProfileCreated(
    const user_manager::User& user) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UpdateSessionAllowedState();
}

void SystemClientCertNetworkObserver::OnUserAffiliationUpdated(
    const user_manager::User& user) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UpdateSessionAllowedState();
}

void SystemClientCertNetworkObserver::OnPolicyProvidedCertsChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  InvalidateAndFlushClientCertCache();
}

void SystemClientCertNetworkObserver::OnPolicyCertificateProviderDestroying() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Defensively detach in case the provider is destroyed before `this` (e.g.,
  // in unit tests or if shutdown ordering changes).
  device_policy_cert_provider_observation_.Reset();
}

bool SystemClientCertNetworkObserver::IsAllowedBySessionState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Before login (OOBE / login screen), `sessions()` is empty and device
  // client certificate auto-selection is allowed.
  for (const auto& session : session_manager_->sessions()) {
    const auto& user =
        CHECK_DEREF(user_manager_->FindUser(session->account_id()));
    if (user.GetType() == user_manager::UserType::kGuest) {
      return false;
    }
    if (!user.is_profile_created()) {
      // Allow during initial profile creation (first-time or ephemeral login
      // temporarily sets `is_affiliated` to `false` before the initial user
      // policy fetch completes).
      continue;
    }
    // `is_affiliated()` is `std::optional<bool>`; keep `std::nullopt` allowed
    // until affiliation is determined, and block only when explicitly `false`.
    if (user.is_affiliated() == false) {
      return false;
    }
  }
  return true;
}

void SystemClientCertNetworkObserver::UpdateSessionAllowedState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const bool is_allowed = IsAllowedBySessionState();
  if (is_allowed != was_allowed_by_session_state_) {
    was_allowed_by_session_state_ = is_allowed;
    InvalidateAndFlushClientCertCache();
  }
}

void SystemClientCertNetworkObserver::InvalidateAndFlushClientCertCache() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ++state_generation_;
  ssl_private_key_receivers_.Clear();
  flush_client_cert_cache_callback_.Run();
}

void SystemClientCertNetworkObserver::OnAutoSelectCertificatePatternsChanged(
    const base::Value* previous,
    const base::Value* current) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  {
    base::AutoLock auto_lock(policy_value_map_.GetLock());
    policy_value_map_.DeleteValues(
        ContentSettingsType::AUTO_SELECT_CERTIFICATE);
    if (current && current->is_list()) {
      content_settings::PolicyProvider::PopulateAutoSelectCertificateSettings(
          current->GetList(), policy_value_map_);
    }
  }
  InvalidateAndFlushClientCertCache();
}

base::ListValue SystemClientCertNetworkObserver::GetAutoSelectionFilters(
    const GURL& requesting_url) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::AutoLock auto_lock(policy_value_map_.GetLock());
  const base::Value* setting =
      policy_value_map_.GetValue(requesting_url, requesting_url,
                                 ContentSettingsType::AUTO_SELECT_CERTIFICATE);
  if (!setting || !setting->is_dict()) {
    return base::ListValue();
  }
  const base::ListValue* filters = setting->GetDict().FindList("filters");
  if (!filters) {
    return base::ListValue();
  }
  return filters->Clone();
}

void SystemClientCertNetworkObserver::OnClientCertsRetrieved(
    uint64_t request_generation,
    base::ListValue filters,
    mojo::PendingRemote<network::mojom::ClientCertificateResponder>
        cert_responder_pending,
    std::unique_ptr<net::ClientCertStore> cert_store,
    net::ClientCertIdentityList client_certs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  mojo::Remote<network::mojom::ClientCertificateResponder> cert_responder(
      std::move(cert_responder_pending));

  if (!IsAllowedBySessionState() || request_generation != state_generation_) {
    cert_responder->CancelRequest();
    return;
  }

  net::ClientCertIdentityList matching_certificates;
  net::ClientCertIdentityList nonmatching_certificates;
  certificate_matching::AutoSelectCertificates(filters, std::move(client_certs),
                                               &matching_certificates,
                                               &nonmatching_certificates);

  if (matching_certificates.empty()) {
    cert_responder->CancelRequest();
    return;
  }

  std::unique_ptr<net::ClientCertIdentity> auto_selected_identity =
      std::move(matching_certificates[0]);
  scoped_refptr<net::X509Certificate> cert =
      auto_selected_identity->certificate();
  net::ClientCertIdentity::SelfOwningAcquirePrivateKey(
      std::move(auto_selected_identity),
      base::BindOnce(&SystemClientCertNetworkObserver::ContinueWithCertificate,
                     weak_factory_.GetWeakPtr(), request_generation,
                     std::move(cert_responder), std::move(cert)));
}

void SystemClientCertNetworkObserver::ContinueWithCertificate(
    uint64_t request_generation,
    mojo::Remote<network::mojom::ClientCertificateResponder> cert_responder,
    scoped_refptr<net::X509Certificate> cert,
    scoped_refptr<net::SSLPrivateKey> private_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsAllowedBySessionState() || request_generation != state_generation_) {
    cert_responder->CancelRequest();
    return;
  }
  if (cert && private_key) {
    mojo::PendingRemote<network::mojom::SSLPrivateKey> ssl_private_key;
    ssl_private_key_receivers_.Add(
        std::make_unique<SSLPrivateKeyWrapper>(private_key),
        ssl_private_key.InitWithNewPipeAndPassReceiver());
    // TODO(crbug.com/578893048): `ContinueWithCertificate` and
    // `NetworkContext::FlushClientCertCache()` travel on separate Mojo pipes
    // (`ssl_private_key_receivers_.Clear()` already revokes signing on flush,
    // preventing unauthorized mTLS handshakes). If dynamic Cert A -> Cert B
    // policy rotation mid-handshake ever matters in practice, order the flush
    // behind in-flight `cert_responder`s via
    // `PauseReceiverUntilFlushCompletes(responder.FlushAsync())`.
    cert_responder->ContinueWithCertificate(
        cert, private_key->GetProviderName(),
        private_key->GetAlgorithmPreferences(), std::move(ssl_private_key));
  } else {
    cert_responder->CancelRequest();
  }
}

}  // namespace ash
