// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_NETWORK_CONTEXT_H_
#define CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_NETWORK_CONTEXT_H_

#include <optional>

#include "base/memory/scoped_refptr.h"
#include "base/unguessable_token.h"
#include "chrome/browser/net/proxy_config_monitor.h"
#include "chrome/browser/profiles/profile.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/auth.h"
#include "net/cert/x509_certificate.h"
#include "net/http/http_response_headers.h"
#include "net/ssl/client_cert_identity.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_private_key.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"
#include "url/gurl.h"

namespace bruschetta {

// Provides an isolated NetworkContext which uses the client certificate store
// from the Profile passed to the constructor, but only if client certificates
// are auto-selected by Profile's enterprise policy.
class BruschettaNetworkContext
    : public network::mojom::URLLoaderNetworkServiceObserver {
 public:
  // Class should not outlive the passed-in profile.
  explicit BruschettaNetworkContext(Profile* profile);

  BruschettaNetworkContext(const BruschettaNetworkContext&) = delete;
  BruschettaNetworkContext& operator=(const BruschettaNetworkContext&) = delete;

  ~BruschettaNetworkContext() override;

  network::mojom::URLLoaderFactory* GetURLLoaderFactory();

 protected:
  // network::mojom::URLLoaderNetworkServiceObserver overrides.
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
      const net::AuthChallengeInfo& auth_info,
      const scoped_refptr<net::HttpResponseHeaders>& head_headers,
      mojo::PendingRemote<network::mojom::AuthChallengeResponder>
          auth_challenge_responder) override;
  void OnPrivateNetworkAccessPermissionRequired(
      const GURL& url,
      const net::IPAddress& ip_address,
      const std::optional<std::string>& private_network_device_id,
      const std::optional<std::string>& private_network_device_name,
      OnPrivateNetworkAccessPermissionRequiredCallback callback) override;
  void OnClearSiteData(
      const GURL& url,
      const std::string& header_value,
      int32_t load_flags,
      const std::optional<net::CookiePartitionKey>& cookie_partition_key,
      bool partitioned_state_allowed_only,
      OnClearSiteDataCallback callback) override;
  void OnLoadingStateUpdate(network::mojom::LoadInfoPtr info,
                            OnLoadingStateUpdateCallback callback) override;
  void OnDataUseUpdate(int32_t network_traffic_annotation_id_hash,
                       int64_t recv_bytes,
                       int64_t sent_bytes) override;
  void OnSharedStorageHeaderReceived(
      const url::Origin& request_origin,
      std::vector<network::mojom::SharedStorageOperationPtr> operations,
      OnSharedStorageHeaderReceivedCallback callback) override;
  void Clone(
      mojo::PendingReceiver<network::mojom::URLLoaderNetworkServiceObserver>
          listener) override;
  void OnWebSocketConnectedToPrivateNetwork(
      network::mojom::IPAddressSpace ip_address_space) override;

 private:
  void ContinueWithCertificate(
      mojo::PendingRemote<network::mojom::ClientCertificateResponder>
          cert_responder_remote,
      scoped_refptr<net::X509Certificate> cert,
      scoped_refptr<net::SSLPrivateKey> private_key);

  void EnsureNetworkContextExists();

  void OnGotClientCerts(
      const scoped_refptr<net::SSLCertRequestInfo>& cert_info,
      mojo::PendingRemote<network::mojom::ClientCertificateResponder>
          cert_responder_remote,
      net::ClientCertIdentityList certs);

  void CreateNetworkContext();

  raw_ptr<Profile> profile_;
  ProxyConfigMonitor proxy_config_monitor_;

  mojo::Remote<network::mojom::NetworkContext> network_context_;

  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_;

  mojo::ReceiverSet<network::mojom::URLLoaderNetworkServiceObserver>
      url_loader_observers_;
  std::unique_ptr<net::ClientCertStore> cert_store_;

  base::WeakPtrFactory<BruschettaNetworkContext> weak_ptr_factory_{this};
};

}  // namespace bruschetta
#endif  // CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_NETWORK_CONTEXT_H_
