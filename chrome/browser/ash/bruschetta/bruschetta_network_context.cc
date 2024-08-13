// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bruschetta/bruschetta_network_context.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/extensions/cws_info_service.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/net/proxy_config_monitor.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/auth.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_certificate.h"
#include "net/http/http_response_headers.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_private_key.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"
#include "url/gurl.h"

namespace bruschetta {

// Wraps a net::SSLPrivateKey and turns it into a network::mojom::SSLPrivateKey.
class SSLPrivateKeyBridge : public network::mojom::SSLPrivateKey {
 public:
  explicit SSLPrivateKeyBridge(
      scoped_refptr<net::SSLPrivateKey> ssl_private_key)
      : ssl_private_key_(std::move(ssl_private_key)) {}

  SSLPrivateKeyBridge(const SSLPrivateKeyBridge&) = delete;
  SSLPrivateKeyBridge& operator=(const SSLPrivateKeyBridge&) = delete;

  ~SSLPrivateKeyBridge() override = default;

  // network::mojom::SSLPrivateKey:
  void Sign(uint16_t algorithm,
            const std::vector<uint8_t>& input,
            network::mojom::SSLPrivateKey::SignCallback callback) override {
    base::span<const uint8_t> input_span(input);
    ssl_private_key_->Sign(
        algorithm, input_span,
        base::BindOnce(&SSLPrivateKeyBridge::Callback,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

 private:
  void Callback(network::mojom::SSLPrivateKey::SignCallback callback,
                net::Error net_error,
                const std::vector<uint8_t>& signature) {
    std::move(callback).Run(static_cast<int32_t>(net_error), signature);
  }

  scoped_refptr<net::SSLPrivateKey> ssl_private_key_;
  base::WeakPtrFactory<SSLPrivateKeyBridge> weak_ptr_factory_{this};
};

BruschettaNetworkContext::BruschettaNetworkContext(Profile* profile)
    : profile_(profile),
      proxy_config_monitor_(g_browser_process->local_state()) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

BruschettaNetworkContext::~BruschettaNetworkContext() = default;

network::mojom::URLLoaderFactory*
BruschettaNetworkContext::GetURLLoaderFactory() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!url_loader_factory_ || !url_loader_factory_.is_connected()) {
    EnsureNetworkContextExists();

    url_loader_observers_.Clear();
    network::mojom::URLLoaderFactoryParamsPtr url_loader_factory_params =
        network::mojom::URLLoaderFactoryParams::New();
    url_loader_factory_params->process_id = network::mojom::kBrowserProcessId;
    url_loader_factory_params->is_orb_enabled = false;
    url_loader_factory_params->is_trusted = true;
    url_loader_observers_.Add(
        this, url_loader_factory_params->url_loader_network_observer
                  .InitWithNewPipeAndPassReceiver());
    url_loader_factory_.reset();
    network_context_->CreateURLLoaderFactory(
        url_loader_factory_.BindNewPipeAndPassReceiver(),
        std::move(url_loader_factory_params));
  }
  return url_loader_factory_.get();
}

void BruschettaNetworkContext::EnsureNetworkContextExists() {
  if (network_context_ && network_context_.is_connected()) {
    return;
  }
  CreateNetworkContext();
}

void BruschettaNetworkContext::CreateNetworkContext() {
  network::mojom::NetworkContextParamsPtr network_context_params =
      g_browser_process->system_network_context_manager()
          ->CreateDefaultNetworkContextParams();
  network_context_params->http_cache_enabled = false;

  proxy_config_monitor_.AddToNetworkContextParams(network_context_params.get());

  network_context_.reset();
  content::CreateNetworkContextInNetworkService(
      network_context_.BindNewPipeAndPassReceiver(),
      std::move(network_context_params));
}

void BruschettaNetworkContext::OnSSLCertificateError(
    const GURL& url,
    int net_error,
    const net::SSLInfo& ssl_info,
    bool fatal,
    OnSSLCertificateErrorCallback response) {
  std::move(response).Run(net::ERR_INSECURE_RESPONSE);
}

void BruschettaNetworkContext::OnCertificateRequested(
    const std::optional<base::UnguessableToken>& window_id,
    const scoped_refptr<net::SSLCertRequestInfo>& cert_info,
    mojo::PendingRemote<network::mojom::ClientCertificateResponder>
        cert_responder_remote) {
  if (!cert_store_) {
    cert_store_ = ProfileNetworkContextServiceFactory::GetForContext(profile_)
                      ->CreateClientCertStore();
  }
  cert_store_->GetClientCerts(
      cert_info, base::BindOnce(&BruschettaNetworkContext::OnGotClientCerts,
                                weak_ptr_factory_.GetWeakPtr(), cert_info,
                                std::move(cert_responder_remote)));
}

void BruschettaNetworkContext::OnGotClientCerts(
    const scoped_refptr<net::SSLCertRequestInfo>& cert_info,
    mojo::PendingRemote<network::mojom::ClientCertificateResponder>
        cert_responder_remote,
    net::ClientCertIdentityList certs) {
  GURL requesting_url =
      enterprise_util::GetRequestingUrl(cert_info->host_and_port);
  net::ClientCertIdentityList matching_certificates, nonmatching_certificates;
  // Bruschetta is an enterprise feature with the URL set in policy. So if they
  // pick a URL which requires an SSL cert they should also provide the cert via
  // policy. We don't have a WebContents so can't show UI.
  enterprise_util::AutoSelectCertificates(
      profile_, requesting_url, std::move(certs), &matching_certificates,
      &nonmatching_certificates);

  if (matching_certificates.size() == 0) {
    LOG(ERROR) << "No matching certificate, continuing without any (this will "
                  "probably fail)";
    mojo::Remote<network::mojom::ClientCertificateResponder> cert_responder(
        std::move(cert_responder_remote));
    cert_responder->ContinueWithoutCertificate();
    return;
  }

  // We're called from views code which doesn't have a WebContents with which
  // to prompt the user to pick a cert, so always take the first matching cert
  // even if there are multiple.
  std::unique_ptr<net::ClientCertIdentity> auto_selected_identity =
      std::move(matching_certificates[0]);

  scoped_refptr<net::X509Certificate> cert =
      auto_selected_identity->certificate();
  net::ClientCertIdentity::SelfOwningAcquirePrivateKey(
      std::move(auto_selected_identity),
      base::BindOnce(&BruschettaNetworkContext::ContinueWithCertificate,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(cert_responder_remote), std::move(cert)));
}

void BruschettaNetworkContext::ContinueWithCertificate(
    mojo::PendingRemote<network::mojom::ClientCertificateResponder>
        cert_responder_remote,
    scoped_refptr<net::X509Certificate> cert,
    scoped_refptr<net::SSLPrivateKey> private_key) {
  mojo::PendingRemote<network::mojom::SSLPrivateKey> ssl_private_key;
  mojo::Remote<network::mojom::ClientCertificateResponder> cert_responder(
      std::move(cert_responder_remote));

  mojo::MakeSelfOwnedReceiver(
      std::make_unique<SSLPrivateKeyBridge>(private_key),
      ssl_private_key.InitWithNewPipeAndPassReceiver());
  cert_responder->ContinueWithCertificate(
      cert, private_key->GetProviderName(),
      private_key->GetAlgorithmPreferences(), std::move(ssl_private_key));
}

void BruschettaNetworkContext::OnAuthRequired(
    const std::optional<base::UnguessableToken>& window_id,
    int32_t request_id,
    const GURL& url,
    bool first_auth_attempt,
    const net::AuthChallengeInfo& auth_info,
    const scoped_refptr<net::HttpResponseHeaders>& head_headers,
    mojo::PendingRemote<network::mojom::AuthChallengeResponder>
        auth_challenge_responder) {
  mojo::Remote<network::mojom::AuthChallengeResponder>
      auth_challenge_responder_remote(std::move(auth_challenge_responder));
  auth_challenge_responder_remote->OnAuthCredentials(std::nullopt);
}

void BruschettaNetworkContext::OnPrivateNetworkAccessPermissionRequired(
    const GURL& url,
    const net::IPAddress& ip_address,
    const std::optional<std::string>& private_network_device_id,
    const std::optional<std::string>& private_network_device_name,
    OnPrivateNetworkAccessPermissionRequiredCallback callback) {
  std::move(callback).Run(false);
}

void BruschettaNetworkContext::OnClearSiteData(
    const GURL& url,
    const std::string& header_value,
    int32_t load_flags,
    const std::optional<net::CookiePartitionKey>& cookie_partition_key,
    bool partitioned_state_allowed_only,
    OnClearSiteDataCallback callback) {
  std::move(callback).Run();
}

void BruschettaNetworkContext::OnLoadingStateUpdate(
    network::mojom::LoadInfoPtr info,
    OnLoadingStateUpdateCallback callback) {
  std::move(callback).Run();
}

void BruschettaNetworkContext::OnDataUseUpdate(
    int32_t network_traffic_annotation_id_hash,
    int64_t recv_bytes,
    int64_t sent_bytes) {}

void BruschettaNetworkContext::OnSharedStorageHeaderReceived(
    const url::Origin& request_origin,
    std::vector<network::mojom::SharedStorageOperationPtr> operations,
    OnSharedStorageHeaderReceivedCallback callback) {
  std::move(callback).Run();
}

void BruschettaNetworkContext::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderNetworkServiceObserver>
        observer) {
  url_loader_observers_.Add(this, std::move(observer));
}

void BruschettaNetworkContext::OnWebSocketConnectedToPrivateNetwork(
    network::mojom::IPAddressSpace ip_address_space) {}

}  // namespace bruschetta
