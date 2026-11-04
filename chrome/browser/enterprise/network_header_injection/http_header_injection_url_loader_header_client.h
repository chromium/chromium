// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_NETWORK_HEADER_INJECTION_HTTP_HEADER_INJECTION_URL_LOADER_HEADER_CLIENT_H_
#define CHROME_BROWSER_ENTERPRISE_NETWORK_HEADER_INJECTION_HTTP_HEADER_INJECTION_URL_LOADER_HEADER_CLIENT_H_

#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace enterprise_custom_headers {
class HttpHeaderInjectionService;

// Implements TrustedURLLoaderHeaderClient to intercept loader creation and bind
// our HttpHeaderInjectionClient.
class HttpHeaderInjectionURLLoaderHeaderClient
    : public network::mojom::TrustedURLLoaderHeaderClient {
 public:
  HttpHeaderInjectionURLLoaderHeaderClient(
      const HttpHeaderInjectionURLLoaderHeaderClient&) = delete;
  HttpHeaderInjectionURLLoaderHeaderClient& operator=(
      const HttpHeaderInjectionURLLoaderHeaderClient&) = delete;
  ~HttpHeaderInjectionURLLoaderHeaderClient() override;

  // Creates an instance and binds it to `receiver`. If `target_client` is
  // valid, this instance acts as a proxy, forwarding calls to it while also
  // ensuring the HttpHeaderInjectionClient is created to inject enterprise
  // headers.
  static void Create(
      base::WeakPtr<HttpHeaderInjectionService> service,
      mojo::PendingReceiver<network::mojom::TrustedURLLoaderHeaderClient>
          receiver,
      mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
          target_client);

  // network::mojom::TrustedURLLoaderHeaderClient:
  void OnLoaderCreated(
      int32_t request_id,
      mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver)
      override;
  void OnLoaderForCorsPreflightCreated(
      const network::ResourceRequest& request,
      mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver)
      override;
  void OnLoaderFailedWithRemoteEndpoint(
      int32_t request_id,
      const net::IPEndPoint& remote_endpoint) override;

 private:
  HttpHeaderInjectionURLLoaderHeaderClient(
      base::WeakPtr<HttpHeaderInjectionService> service,
      mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
          target_client);

  void OnTargetDisconnect();

  base::WeakPtr<HttpHeaderInjectionService> service_;
  mojo::Remote<network::mojom::TrustedURLLoaderHeaderClient> target_client_;
};

}  // namespace enterprise_custom_headers

#endif  // CHROME_BROWSER_ENTERPRISE_NETWORK_HEADER_INJECTION_HTTP_HEADER_INJECTION_URL_LOADER_HEADER_CLIENT_H_
