// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SITE_TOKEN_PROVIDER_SITE_TOKEN_URL_LOADER_FACTORY_H_
#define CHROME_BROWSER_SITE_TOKEN_PROVIDER_SITE_TOKEN_URL_LOADER_FACTORY_H_

#include <cstdint>

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace net {
struct MutableNetworkTrafficAnnotationTag;
}  // namespace net

namespace network {
struct ResourceRequest;
}  // namespace network

namespace site_token_provider {

// URLLoaderFactory for the `chrome-experimental-site-token` scheme.
// Intercepts subresource requests from renderers and returns domain-bound
// site tokens with appropriate headers.
class SiteTokenURLLoaderFactory : public network::mojom::URLLoaderFactory {
 public:
  static mojo::PendingRemote<network::mojom::URLLoaderFactory> Create(
      int render_process_id);

  explicit SiteTokenURLLoaderFactory(int render_process_id);
  ~SiteTokenURLLoaderFactory() override;

  SiteTokenURLLoaderFactory(const SiteTokenURLLoaderFactory&) = delete;
  SiteTokenURLLoaderFactory& operator=(const SiteTokenURLLoaderFactory&) =
      delete;

  // network::mojom::URLLoaderFactory:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override;

 private:
  const int render_process_id_;
};

}  // namespace site_token_provider

#endif  // CHROME_BROWSER_SITE_TOKEN_PROVIDER_SITE_TOKEN_URL_LOADER_FACTORY_H_
