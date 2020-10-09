// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOADER_URL_LOADER_FACTORY_PROXY_IMPL_H_
#define CHROME_BROWSER_LOADER_URL_LOADER_FACTORY_PROXY_IMPL_H_

#include "chrome/common/url_loader_factory_proxy.mojom.h"
#include "content/public/browser/global_routing_id.h"

namespace content {
class RenderFrameHost;
}  // namespace content

// This class is currently used by subresource loading for Web Bundles to proxy
// requests, so that subresource requests that are served within the renderer
// process (from a Web Bundle) can still be intercepted by Chrome extensions.
// This interface is implemented only when ENABLE_EXTENSIONS build flag is set.
// TODO(crbug.com/1135829): Consider using RenderDocumentHostUserData to
// restrict the lifetime of this object to the lifetime of a navigation.
class UrlLoaderFactoryProxyImpl : public chrome::mojom::UrlLoaderFactoryProxy {
 public:
  static void Create(
      content::RenderFrameHost* frame_host,
      mojo::PendingReceiver<chrome::mojom::UrlLoaderFactoryProxy> receiver);

  explicit UrlLoaderFactoryProxyImpl(content::RenderFrameHost* frame_host);
  ~UrlLoaderFactoryProxyImpl() override;
  void GetProxiedURLLoaderFactory(
      mojo::PendingRemote<network::mojom::URLLoaderFactory> original_factory,
      mojo::PendingReceiver<::network::mojom::URLLoaderFactory> proxied_factory)
      override;

 private:
  const content::GlobalFrameRoutingId frame_id_;
};

#endif  // CHROME_BROWSER_LOADER_URL_LOADER_FACTORY_PROXY_IMPL_H_
