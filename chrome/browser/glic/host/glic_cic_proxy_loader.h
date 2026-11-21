// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_CIC_PROXY_LOADER_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_CIC_PROXY_LOADER_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <string_view>

#include "base/memory/scoped_refptr.h"
#include "base/memory/self_deleting.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/content_browser_client.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/self_deleting_url_loader_factory.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/url_loader_factory_builder.h"
#include "services/network/public/mojom/url_loader.mojom-forward.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

class Profile;

namespace content {
class RenderFrameHost;
}  // namespace content

namespace network {
struct ResourceRequest;
}  // namespace network

namespace glic {

// Returns the upstream CiC server origin URL from `features::kGlicCicUrl` when
// it is a valid HTTPS or localhost HTTP URL without credentials, or falls back
// to the default production origin (`https://glic-cic.apps.corp.google.com/`).
GURL GetGlicCicBaseUrl();

// Rewrites a same-origin Glic guest `request_url` to the corresponding upstream
// CiC target URL on `cic_base_url`, or returns `std::nullopt` if the request
// should not be proxied:
// - Normalizes main-frame `/glic` navigations (paths matching `guest_path` or
//   ending in `/glic` or `/glic/`, including multi-login paths such as
//   `/u/0/glic`) to `/glic` unless `glic_passthrough=1` is present.
// - Normalizes `/cic/` navigation and subresource paths (including multi-login
//   paths such as `/u/0/cic/...`) to start at `/cic/`.
// - Preserves query parameters and clears ref fragments.
// - Rejects cross-origin URLs, non-GET/HEAD navigations, cross-origin or
//   missing subresource `request_initiator`, and path traversal tokens.
std::optional<GURL> RewriteGlicCicTargetUrl(
    const GURL& request_url,
    const url::Origin& guest_origin,
    std::string_view guest_path,
    const GURL& cic_base_url,
    content::ContentBrowserClient::URLLoaderFactoryType type,
    bool is_primary_main_frame,
    std::string_view method,
    const std::optional<url::Origin>& request_initiator);

// Proxying URLLoaderFactory for frames in the Glic guest primary page in
// `GlicNoWebview` mode when `features::kGlicCic` is enabled:
// - On `kNavigation`: intercepts `GET`/`HEAD` requests on the Glic guest origin
//   whose path matches `guest_path_` or ends in `/glic` or `/glic/` (primary
//   main frame only, unless `glic_passthrough=1`), as well as `/cic/`
//   navigations across frames in the guest primary page, serving responses from
//   `GetGlicCicBaseUrl()` while keeping Blink's committed origin at the Glic
//   guest origin.
// - On `kDocumentSubResource`: intercepts requests whose path contains `/cic/`
//   on the Glic guest origin (requiring matching `request_initiator`) and
//   proxies them to `GetGlicCicBaseUrl()` using the profile's default
//   `StoragePartition` URLLoaderFactory (so corp SSO cookies apply to the CiC
//   origin without leaking Gemini cookies).
// - Follows silent corp SSO redirects back to the CiC origin; if corp SSO
//   requires interactive login, returns a minimal local recovery HTML link
//   (`kNavigation`) or `401 Unauthorized` JSON (`kDocumentSubResource`) rather
//   than redirecting Blink out of the Glic guest origin.
class GlicCicProxyingURLLoaderFactory
    : public network::SelfDeletingURLLoaderFactory {
 public:
  GlicCicProxyingURLLoaderFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> loader_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          target_factory_remote,
      base::WeakPtr<Profile> profile,
      content::ContentBrowserClient::URLLoaderFactoryType type,
      bool is_primary_main_frame,
      url::Origin guest_origin,
      std::string guest_path,
      GURL cic_base_url,
      base::SelfDeletingPassKey pass_key);

  GlicCicProxyingURLLoaderFactory(const GlicCicProxyingURLLoaderFactory&) =
      delete;
  GlicCicProxyingURLLoaderFactory& operator=(
      const GlicCicProxyingURLLoaderFactory&) = delete;

  static void MaybeProxyRequest(
      content::RenderFrameHost* frame,
      content::ContentBrowserClient::URLLoaderFactoryType type,
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

  void SetUpstreamFactoryForTesting(
      scoped_refptr<network::SharedURLLoaderFactory> upstream_factory);

 private:
  ~GlicCicProxyingURLLoaderFactory() override;
  void OnTargetFactoryError();

  mojo::Remote<network::mojom::URLLoaderFactory> target_factory_;
  base::WeakPtr<Profile> profile_;
  scoped_refptr<network::SharedURLLoaderFactory> upstream_factory_for_testing_;
  const content::ContentBrowserClient::URLLoaderFactoryType type_;
  const bool is_primary_main_frame_;
  const url::Origin guest_origin_;
  const std::string guest_path_;
  const GURL cic_base_url_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_CIC_PROXY_LOADER_H_
