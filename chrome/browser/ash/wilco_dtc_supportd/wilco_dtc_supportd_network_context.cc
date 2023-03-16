// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/wilco_dtc_supportd/wilco_dtc_supportd_network_context.h"

#include <memory>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "services/network/network_service.h"

namespace ash {

WilcoDtcSupportdNetworkContextImpl::WilcoDtcSupportdNetworkContextImpl()
    : proxy_config_monitor_(g_browser_process->local_state()) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

WilcoDtcSupportdNetworkContextImpl::~WilcoDtcSupportdNetworkContextImpl() =
    default;

network::mojom::URLLoaderFactory*
WilcoDtcSupportdNetworkContextImpl::GetURLLoaderFactory() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!url_loader_factory_ || !url_loader_factory_.is_connected()) {
    EnsureNetworkContextExists();

    cert_receivers_.Clear();
    network::mojom::URLLoaderFactoryParamsPtr url_loader_factory_params =
        network::mojom::URLLoaderFactoryParams::New();
    url_loader_factory_params->process_id = network::mojom::kBrowserProcessId;
    url_loader_factory_params->is_corb_enabled = false;
    url_loader_factory_params->is_trusted = true;
    cert_receivers_.Add(this,
                        url_loader_factory_params->url_loader_network_observer
                            .InitWithNewPipeAndPassReceiver());
    url_loader_factory_.reset();
    network_context_->CreateURLLoaderFactory(
        url_loader_factory_.BindNewPipeAndPassReceiver(),
        std::move(url_loader_factory_params));
  }
  return url_loader_factory_.get();
}

void WilcoDtcSupportdNetworkContextImpl::FlushForTesting() {
  if (network_context_) {
    network_context_.FlushForTesting();
  }
  if (url_loader_factory_) {
    url_loader_factory_.FlushForTesting();
  }
}

void WilcoDtcSupportdNetworkContextImpl::EnsureNetworkContextExists() {
  if (network_context_ && network_context_.is_connected()) {
    return;
  }
  CreateNetworkContext();
}

void WilcoDtcSupportdNetworkContextImpl::CreateNetworkContext() {
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

void WilcoDtcSupportdNetworkContextImpl::OnSSLCertificateError(
    const GURL& url,
    int net_error,
    const net::SSLInfo& ssl_info,
    bool fatal,
    OnSSLCertificateErrorCallback response) {
  std::move(response).Run(net::ERR_INSECURE_RESPONSE);
}

void WilcoDtcSupportdNetworkContextImpl::OnCertificateRequested(
    const absl::optional<base::UnguessableToken>& window_id,
    const scoped_refptr<net::SSLCertRequestInfo>& cert_info,
    mojo::PendingRemote<network::mojom::ClientCertificateResponder>
        cert_responder_remote) {
  mojo::Remote<network::mojom::ClientCertificateResponder> cert_responder(
      std::move(cert_responder_remote));
  cert_responder->ContinueWithoutCertificate();
}

void WilcoDtcSupportdNetworkContextImpl::OnAuthRequired(
    const absl::optional<base::UnguessableToken>& window_id,
    uint32_t request_id,
    const GURL& url,
    bool first_auth_attempt,
    const net::AuthChallengeInfo& auth_info,
    const scoped_refptr<net::HttpResponseHeaders>& head_headers,
    mojo::PendingRemote<network::mojom::AuthChallengeResponder>
        auth_challenge_responder) {
  mojo::Remote<network::mojom::AuthChallengeResponder>
      auth_challenge_responder_remote(std::move(auth_challenge_responder));
  auth_challenge_responder_remote->OnAuthCredentials(absl::nullopt);
}

void WilcoDtcSupportdNetworkContextImpl::OnClearSiteData(
    const GURL& url,
    const std::string& header_value,
    int32_t load_flags,
    const absl::optional<net::CookiePartitionKey>& cookie_partition_key,
    bool partitioned_state_allowed_only,
    OnClearSiteDataCallback callback) {
  std::move(callback).Run();
}

void WilcoDtcSupportdNetworkContextImpl::OnLoadingStateUpdate(
    network::mojom::LoadInfoPtr info,
    OnLoadingStateUpdateCallback callback) {
  std::move(callback).Run();
}

void WilcoDtcSupportdNetworkContextImpl::OnDataUseUpdate(
    int32_t network_traffic_annotation_id_hash,
    int64_t recv_bytes,
    int64_t sent_bytes) {}

void WilcoDtcSupportdNetworkContextImpl::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderNetworkServiceObserver>
        observer) {
  cert_receivers_.Add(this, std::move(observer));
}

}  // namespace ash
