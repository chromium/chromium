// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SITE_TOKEN_PROVIDER_SITE_TOKEN_URL_LOADER_HEADER_CLIENT_H_
#define CHROME_BROWSER_SITE_TOKEN_PROVIDER_SITE_TOKEN_URL_LOADER_HEADER_CLIENT_H_

#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace content {
class BrowserContext;
}

namespace network {
struct ResourceRequest;
}

namespace site_token_provider {
class SiteTokenProviderService;

// Implements TrustedURLLoaderHeaderClient to intercept loader creation and bind
// our SiteTokenHeaderClient.
class SiteTokenURLLoaderHeaderClient
    : public network::mojom::TrustedURLLoaderHeaderClient {
 public:
  SiteTokenURLLoaderHeaderClient(const SiteTokenURLLoaderHeaderClient&) =
      delete;
  SiteTokenURLLoaderHeaderClient& operator=(
      const SiteTokenURLLoaderHeaderClient&) = delete;
  ~SiteTokenURLLoaderHeaderClient() override;

  static void MaybeWrap(
      content::BrowserContext* browser_context,
      mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
          header_client);

  // network::mojom::TrustedURLLoaderHeaderClient:
  void OnLoaderCreated(
      int32_t request_id,
      mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver)
      override;
  void OnLoaderForCorsPreflightCreated(
      const network::ResourceRequest& request,
      mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver)
      override;

 private:
  static void Create(
      base::WeakPtr<SiteTokenProviderService> service,
      mojo::PendingReceiver<network::mojom::TrustedURLLoaderHeaderClient>
          receiver,
      mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
          target_client);

  SiteTokenURLLoaderHeaderClient(
      base::WeakPtr<SiteTokenProviderService> service,
      mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
          target_client);

  void OnTargetDisconnect();

  base::WeakPtr<SiteTokenProviderService> service_;
  mojo::Remote<network::mojom::TrustedURLLoaderHeaderClient> target_client_;
};

}  // namespace site_token_provider

#endif  // CHROME_BROWSER_SITE_TOKEN_PROVIDER_SITE_TOKEN_URL_LOADER_HEADER_CLIENT_H_
