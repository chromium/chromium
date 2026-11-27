// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_SYSTEM_CLIENT_CERT_NETWORK_OBSERVER_H_
#define CHROME_BROWSER_ASH_NET_SYSTEM_CLIENT_CERT_NETWORK_OBSERVER_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>

#include "base/byte_size.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "chromeos/ash/components/network/policy_certificate_provider.h"
#include "components/content_settings/core/browser/content_settings_origin_value_map.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_manager.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "net/ssl/client_cert_identity.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"

class GURL;

namespace net {
class AuthChallengeInfo;
class ClientCertStore;
class HttpResponseHeaders;
class SSLCertRequestInfo;
class SSLInfo;
class SSLPrivateKey;
class X509Certificate;
}  // namespace net

namespace policy {
class PolicyChangeRegistrar;
class PolicyService;
}  // namespace policy

namespace ash {

// Network service observer for the System NetworkContext on ChromeOS.
// Handles client certificate requests using device-wide Kcer certificates and
// the `DeviceLoginScreenAutoSelectCertificateForUrls` policy when allowed by
// session state (login screen/OOBE and affiliated user sessions), and provides
// default responses matching `content::NetworkServiceClient` for all other
// observer calls.
//
// This class must be created, used, and destroyed on the UI thread.
//
// TODO(b/586891897): Extract a shared default `URLLoaderNetworkServiceObserver`
// base class instead of duplicating `content::NetworkServiceClient` defaults.
class SystemClientCertNetworkObserver
    : public network::mojom::URLLoaderNetworkServiceObserver,
      public user_manager::UserManager::Observer,
      public PolicyCertificateProvider::Observer {
 public:
  // TODO(crbug.com/332804822): Remove `flush_client_cert_cache_callback` and
  // `DataUseCallback` injections once their callees
  // (`SystemNetworkContextManager` and `task_manager::TaskManagerInterface`)
  // are decoupled from `//chrome` (or `DataUseCallback` is subsumed by
  // crbug.com/586891897).
  using DataUseCallback =
      base::RepeatingCallback<void(int32_t network_traffic_annotation_id_hash,
                                   base::ByteSize recv_bytes,
                                   base::ByteSize sent_bytes)>;

  using ClientCertStoreFactory =
      base::RepeatingCallback<std::unique_ptr<net::ClientCertStore>(
          net::CertificateList device_onc_certs)>;

  static ClientCertStoreFactory CreateDefaultClientCertStoreFactory();

  SystemClientCertNetworkObserver(
      session_manager::SessionManager* session_manager,
      user_manager::UserManager* user_manager,
      policy::PolicyService* device_policy_service,
      PolicyCertificateProvider* device_policy_cert_provider,
      base::RepeatingClosure flush_client_cert_cache_callback,
      DataUseCallback data_use_callback,
      ClientCertStoreFactory client_cert_store_factory);

  SystemClientCertNetworkObserver(const SystemClientCertNetworkObserver&) =
      delete;
  SystemClientCertNetworkObserver& operator=(
      const SystemClientCertNetworkObserver&) = delete;

  ~SystemClientCertNetworkObserver() override;

  // Binds `receiver` to this observer's receiver set.
  void BindReceiver(
      mojo::PendingReceiver<network::mojom::URLLoaderNetworkServiceObserver>
          receiver);

  // network::mojom::URLLoaderNetworkServiceObserver:
  void OnSSLCertificateError(const GURL& url,
                             int net_error,
                             const net::SSLInfo& ssl_info,
                             bool fatal,
                             OnSSLCertificateErrorCallback response) override;
  void OnCertificateRequested(
      const std::optional<base::UnguessableToken>& window_id,
      const scoped_refptr<net::SSLCertRequestInfo>& cert_info,
      mojo::PendingRemote<network::mojom::ClientCertificateResponder>
          cert_responder) override;
  void OnAuthRequired(
      const std::optional<base::UnguessableToken>& window_id,
      int32_t request_id,
      const GURL& url,
      bool first_auth_attempt,
      bool do_not_prompt_for_login,
      const net::AuthChallengeInfo& auth_info,
      const scoped_refptr<net::HttpResponseHeaders>& head_headers,
      mojo::PendingRemote<network::mojom::AuthChallengeResponder>
          auth_challenge_responder) override;
  void OnLocalNetworkAccessPermissionRequired(
      network::mojom::TransportType type,
      network::mojom::IPAddressSpace ip_address_space,
      OnLocalNetworkAccessPermissionRequiredCallback callback) override;
  void OnPlatformLocalNetworkPermissionRequired(
      OnPlatformLocalNetworkPermissionRequiredCallback callback) override;
  void OnClearSiteData(
      const GURL& url,
      const std::optional<net::SchemefulSite>& top_level_site,
      const std::optional<base::UnguessableToken>& nonce,
      const std::string& header_value,
      int load_flags,
      const std::optional<net::CookiePartitionKey>& cookie_partition_key,
      bool partitioned_state_allowed_only,
      OnClearSiteDataCallback callback) override;
  void OnLoadingStateUpdate(network::mojom::LoadInfoPtr info,
                            OnLoadingStateUpdateCallback callback) override;
  void OnDataUseUpdate(int32_t network_traffic_annotation_id_hash,
                       base::ByteSize recv_bytes,
                       base::ByteSize sent_bytes) override;
  void Clone(
      mojo::PendingReceiver<network::mojom::URLLoaderNetworkServiceObserver>
          listener) override;
  void OnWebSocketConnectedToLocalNetwork(
      const GURL& request_url,
      network::mojom::IPAddressSpace ip_address_space) override;
  void OnUrlLoaderConnectedToLocalNetwork(
      const GURL& request_url,
      network::mojom::IPAddressSpace response_address_space,
      network::mojom::IPAddressSpace client_address_space,
      network::mojom::IPAddressSpace target_address_space) override;

  // user_manager::UserManager::Observer:
  void OnUserLoggedIn(const user_manager::User& user) override;
  void OnUserProfileCreated(const user_manager::User& user) override;
  void OnUserAffiliationUpdated(const user_manager::User& user) override;

  // PolicyCertificateProvider::Observer:
  void OnPolicyProvidedCertsChanged() override;
  void OnPolicyCertificateProviderDestroying() override;

 private:
  bool IsAllowedBySessionState() const;
  void UpdateSessionAllowedState();
  void InvalidateAndFlushClientCertCache();
  void OnAutoSelectCertificatePatternsChanged(const base::Value* previous,
                                              const base::Value* current);

  base::ListValue GetAutoSelectionFilters(const GURL& requesting_url) const;

  void OnClientCertsRetrieved(
      uint64_t request_generation,
      base::ListValue filters,
      mojo::PendingRemote<network::mojom::ClientCertificateResponder>
          cert_responder_pending,
      std::unique_ptr<net::ClientCertStore> cert_store,
      net::ClientCertIdentityList client_certs);

  void ContinueWithCertificate(
      uint64_t request_generation,
      mojo::Remote<network::mojom::ClientCertificateResponder> cert_responder,
      scoped_refptr<net::X509Certificate> cert,
      scoped_refptr<net::SSLPrivateKey> private_key);

  SEQUENCE_CHECKER(sequence_checker_);

  const raw_ref<session_manager::SessionManager> session_manager_;
  const raw_ref<user_manager::UserManager> user_manager_;
  base::RepeatingClosure flush_client_cert_cache_callback_;
  DataUseCallback data_use_callback_;
  ClientCertStoreFactory client_cert_store_factory_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<policy::PolicyChangeRegistrar> policy_change_registrar_
      GUARDED_BY_CONTEXT(sequence_checker_);
  // Accessed exclusively on `sequence_checker_`. Callers still acquire
  // `policy_value_map_.GetLock()` (uncontended) to satisfy `OriginValueMap` and
  // `PolicyProvider::PopulateAutoSelectCertificateSettings`
  // `EXCLUSIVE_LOCKS_REQUIRED` annotations.
  content_settings::OriginValueMap policy_value_map_
      GUARDED_BY_CONTEXT(sequence_checker_);
  // Last known result of `IsAllowedBySessionState()`, used to detect session
  // state transitions that require flushing cached client certificates.
  bool was_allowed_by_session_state_ GUARDED_BY_CONTEXT(sequence_checker_);
  // Incremented whenever session allowed state, auto-selection filters, or
  // policy-provided certificates change and the client cert cache is flushed,
  // to invalidate in-flight asynchronous certificate/key lookups started under
  // a previous state.
  uint64_t state_generation_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;
  base::ScopedObservation<user_manager::UserManager,
                          user_manager::UserManager::Observer>
      user_manager_observation_{this};
  // Also reset defensively in `OnPolicyCertificateProviderDestroying()` in case
  // the provider is destroyed before `this`.
  base::ScopedObservation<PolicyCertificateProvider,
                          PolicyCertificateProvider::Observer>
      device_policy_cert_provider_observation_{this};
  mojo::ReceiverSet<network::mojom::URLLoaderNetworkServiceObserver> receivers_
      GUARDED_BY_CONTEXT(sequence_checker_);
  mojo::UniqueReceiverSet<network::mojom::SSLPrivateKey>
      ssl_private_key_receivers_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<SystemClientCertNetworkObserver> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NET_SYSTEM_CLIENT_CERT_NETWORK_OBSERVER_H_
