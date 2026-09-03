// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SITE_TOKEN_PROVIDER_SITE_TOKEN_PROXYING_URL_LOADER_FACTORY_H_
#define CHROME_BROWSER_SITE_TOKEN_PROVIDER_SITE_TOKEN_PROXYING_URL_LOADER_FACTORY_H_

#include <cstdint>

#include "base/functional/callback.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/self_deleting_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}

namespace net {
struct MutableNetworkTrafficAnnotationTag;
}

namespace network {
class URLLoaderFactoryBuilder;
struct ResourceRequest;
}  // namespace network

namespace site_token_provider {

// Proxies `URLLoaderFactory` to force the use of `TrustedHeaderClient` for
// requests to allowlisted domains.
class SiteTokenProxyingURLLoaderFactory
    : public network::SelfDeletingURLLoaderFactory {
 public:
  using CheckUrlCallback = base::RepeatingCallback<bool(const GURL&)>;

  SiteTokenProxyingURLLoaderFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> loader_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          target_factory_remote,
      CheckUrlCallback check_url_callback,
      base::SelfDeletingPassKey pass_key);

  SiteTokenProxyingURLLoaderFactory(const SiteTokenProxyingURLLoaderFactory&) =
      delete;
  SiteTokenProxyingURLLoaderFactory& operator=(
      const SiteTokenProxyingURLLoaderFactory&) = delete;

  // Intercepts `factory_builder` if `browser_context` supports site tokens.
  static void MaybeProxyRequest(
      content::BrowserContext* browser_context,
      network::URLLoaderFactoryBuilder& factory_builder);

  // network::mojom::URLLoaderFactory:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;

 private:
  ~SiteTokenProxyingURLLoaderFactory() override;

  void OnTargetFactoryError();

  mojo::Remote<network::mojom::URLLoaderFactory> target_factory_;
  const CheckUrlCallback check_url_callback_;
};

}  // namespace site_token_provider

#endif  // CHROME_BROWSER_SITE_TOKEN_PROVIDER_SITE_TOKEN_PROXYING_URL_LOADER_FACTORY_H_
