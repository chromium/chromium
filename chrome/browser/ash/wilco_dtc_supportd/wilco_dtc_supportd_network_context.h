// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WILCO_DTC_SUPPORTD_WILCO_DTC_SUPPORTD_NETWORK_CONTEXT_H_
#define CHROME_BROWSER_ASH_WILCO_DTC_SUPPORTD_WILCO_DTC_SUPPORTD_NETWORK_CONTEXT_H_

#include "base/memory/scoped_refptr.h"
#include "base/unguessable_token.h"
#include "chrome/browser/net/proxy_config_monitor.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/auth.h"
#include "net/http/http_response_headers.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace ash {

class WilcoDtcSupportdNetworkContext {
 public:
  virtual ~WilcoDtcSupportdNetworkContext() = default;

  virtual network::mojom::URLLoaderFactory* GetURLLoaderFactory() = 0;
};

class WilcoDtcSupportdNetworkContextImpl
    : public WilcoDtcSupportdNetworkContext,
      public network::mojom::URLLoaderNetworkServiceObserver {
 public:
  WilcoDtcSupportdNetworkContextImpl();

  WilcoDtcSupportdNetworkContextImpl(
      const WilcoDtcSupportdNetworkContextImpl&) = delete;
  WilcoDtcSupportdNetworkContextImpl& operator=(
      const WilcoDtcSupportdNetworkContextImpl&) = delete;

  ~WilcoDtcSupportdNetworkContextImpl() override;

  // WilcoDtcSupportdNetworkContext overrides:
  network::mojom::URLLoaderFactory* GetURLLoaderFactory() override;

  void FlushForTesting();

 private:
  // Ensures that Network Context created and connected to the network service.
  void EnsureNetworkContextExists();

  // Creates Network Context.
  void CreateNetworkContext();

  // network::mojom::URLLoaderNetworkServiceObserver interface.
  void OnSSLCertificateError(const GURL& url,
                             int net_error,
                             const net::SSLInfo& ssl_info,
                             bool fatal,
                             OnSSLCertificateErrorCallback response) override;
  void OnCertificateRequested(
      const absl::optional<base::UnguessableToken>& window_id,
      const scoped_refptr<net::SSLCertRequestInfo>& cert_info,
      mojo::PendingRemote<network::mojom::ClientCertificateResponder>
          cert_responder) override;
  void OnAuthRequired(
      const absl::optional<base::UnguessableToken>& window_id,
      uint32_t request_id,
      const GURL& url,
      bool first_auth_attempt,
      const net::AuthChallengeInfo& auth_info,
      const scoped_refptr<net::HttpResponseHeaders>& head_headers,
      mojo::PendingRemote<network::mojom::AuthChallengeResponder>
          auth_challenge_responder) override;
  void OnClearSiteData(
      const GURL& url,
      const std::string& header_value,
      int32_t load_flags,
      const absl::optional<net::CookiePartitionKey>& cookie_partition_key,
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

  ProxyConfigMonitor proxy_config_monitor_;

  // NetworkContext using the network service.
  mojo::Remote<network::mojom::NetworkContext> network_context_;

  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_;

  mojo::ReceiverSet<network::mojom::URLLoaderNetworkServiceObserver>
      cert_receivers_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_WILCO_DTC_SUPPORTD_WILCO_DTC_SUPPORTD_NETWORK_CONTEXT_H_
