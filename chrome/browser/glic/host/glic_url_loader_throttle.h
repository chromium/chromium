// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_URL_LOADER_THROTTLE_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_URL_LOADER_THROTTLE_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/global_routing_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/self_deleting_url_loader_factory.h"
#include "services/network/public/cpp/url_loader_factory_builder.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "url/origin.h"

class Profile;

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace network {
struct ResourceRequest;
}  // namespace network

namespace net {
class HttpRequestHeaders;
struct RedirectInfo;
}  // namespace net

namespace glic {

// Throttle that injects Glic custom HTTP request headers (X-Glic,
// X-Glic-Chrome-Version, X-Glic-Chrome-Channel) on requests originating from
// the Glic guest WebContents when running in NoWebview mode.
class GlicURLLoaderThrottle : public blink::URLLoaderThrottle {
 public:
  static std::unique_ptr<GlicURLLoaderThrottle> MaybeCreate(
      Profile* profile,
      const base::RepeatingCallback<content::WebContents*()>& wc_getter,
      content::FrameTreeNodeId frame_tree_node_id,
      const network::ResourceRequest& request);

  GlicURLLoaderThrottle();
  explicit GlicURLLoaderThrottle(base::WeakPtr<Profile> profile);
  ~GlicURLLoaderThrottle() override;

  GlicURLLoaderThrottle(const GlicURLLoaderThrottle&) = delete;
  GlicURLLoaderThrottle& operator=(const GlicURLLoaderThrottle&) = delete;

  // blink::URLLoaderThrottle:
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;
  void WillRedirectRequest(
      net::RedirectInfo* redirect_info,
      const network::mojom::URLResponseHead& response_head,
      bool* defer,
      network::HttpRequestHeadersUpdateParams* headers_update_params) override;

  // Injects Glic custom request headers into `headers`.
  static void SetHeaders(net::HttpRequestHeaders* headers, Profile* profile);

 private:
  base::WeakPtr<Profile> profile_;
};

// Proxying URLLoaderFactory for subresource requests issued by the Glic guest
// primary main frame in NoWebview mode:
// - Serves `chrome__glic_api.js` (relative to the guest document) directly from
//   browser memory with hash-based V8 bytecode caching enabled when
//   `GlicProviderSpec::SupportsApiBundleResource()` is true and the origin is
//   allowed the Glic API.
// - Injects Glic custom HTTP request headers on network subresource requests
//   (fetch, XHR, WebSocket) when `kGlicHeader` is enabled.
class GlicSubresourceProxyingURLLoaderFactory
    : public network::SelfDeletingURLLoaderFactory {
 public:
  GlicSubresourceProxyingURLLoaderFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> loader_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          target_factory_remote,
      base::WeakPtr<Profile> profile,
      content::GlobalRenderFrameHostId frame_id,
      url::Origin initiator,
      bool inject_headers,
      bool serve_api_bundle,
      base::SelfDeletingPassKey pass_key);

  GlicSubresourceProxyingURLLoaderFactory(
      const GlicSubresourceProxyingURLLoaderFactory&) = delete;
  GlicSubresourceProxyingURLLoaderFactory& operator=(
      const GlicSubresourceProxyingURLLoaderFactory&) = delete;

  static void MaybeProxyRequest(
      content::RenderFrameHost* frame,
      const url::Origin& request_initiator,
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
  ~GlicSubresourceProxyingURLLoaderFactory() override;
  void OnTargetFactoryError();

  bool IsBundleRequest(const network::ResourceRequest& request) const;
  void HandleBundleRequest(
      mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client);

  mojo::Remote<network::mojom::URLLoaderFactory> target_factory_;
  base::WeakPtr<Profile> profile_;
  const content::GlobalRenderFrameHostId frame_id_;
  const url::Origin initiator_;
  const bool inject_headers_;
  const bool serve_api_bundle_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_URL_LOADER_THROTTLE_H_
