// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SITE_TOKEN_PROVIDER_SITE_TOKEN_HEADER_CLIENT_H_
#define CHROME_BROWSER_SITE_TOKEN_PROVIDER_SITE_TOKEN_HEADER_CLIENT_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/ip_endpoint.h"
#include "net/http/http_request_headers.h"
#include "net/ssl/ssl_info.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"

namespace site_token_provider {
class SiteTokenProviderService;

// Implements TrustedHeaderClient to inject secure site tokens.
// Supports chaining with other TrustedHeaderClients.
class SiteTokenHeaderClient : public network::mojom::TrustedHeaderClient {
 public:
  static void Create(
      base::WeakPtr<SiteTokenProviderService> service,
      mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver,
      mojo::PendingRemote<network::mojom::TrustedHeaderClient> target_client);

  SiteTokenHeaderClient(const SiteTokenHeaderClient&) = delete;
  SiteTokenHeaderClient& operator=(const SiteTokenHeaderClient&) = delete;
  ~SiteTokenHeaderClient() override;

  // network::mojom::TrustedHeaderClient:
  void OnBeforeSendHeaders(const GURL& request_url,
                           const net::HttpRequestHeaders& headers,
                           OnBeforeSendHeadersCallback callback) override;
  void OnHeadersReceived(const std::string& headers,
                         const net::IPEndPoint& remote_endpoint,
                         const std::optional<net::SSLInfo>& ssl_info,
                         OnHeadersReceivedCallback callback) override;

 private:
  SiteTokenHeaderClient(
      base::WeakPtr<SiteTokenProviderService> service,
      mojo::PendingRemote<network::mojom::TrustedHeaderClient> target_client);

  void OnTargetDisconnect();

  void OnTargetBeforeSendHeadersComplete(
      OnBeforeSendHeadersCallback callback,
      const GURL& request_url,
      const net::HttpRequestHeaders& original_headers,
      int32_t result,
      const std::optional<net::HttpRequestHeaders>& headers,
      std::optional<base::DictValue> extended_net_log_events);

  base::WeakPtr<SiteTokenProviderService> service_;
  mojo::Remote<network::mojom::TrustedHeaderClient> target_client_;

  base::WeakPtrFactory<SiteTokenHeaderClient> weak_ptr_factory_{this};
};

}  // namespace site_token_provider

#endif  // CHROME_BROWSER_SITE_TOKEN_PROVIDER_SITE_TOKEN_HEADER_CLIENT_H_
