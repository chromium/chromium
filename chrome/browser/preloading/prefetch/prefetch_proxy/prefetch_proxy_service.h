// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_SERVICE_H_
#define CHROME_BROWSER_PRELOADING_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_SERVICE_H_

#include <map>
#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/content_browser_client.h"
#include "url/gurl.h"

class Profile;
class PrefetchProxyProxyConfigurator;
class PrefetchProxyOriginProber;
class PrefetchProxyOriginDecider;
class PrefetchProxySubresourceManager;
class PrefetchedMainframeResponseContainer;

namespace content {
class RenderFrameHost;
}

// This service owns browser-level objects used in Prefetch Proxy.
class PrefetchProxyService : public KeyedService {
 public:
  explicit PrefetchProxyService(Profile* profile);
  ~PrefetchProxyService() override;

  PrefetchProxyProxyConfigurator* proxy_configurator() {
    return proxy_configurator_.get();
  }

  PrefetchProxyOriginProber* origin_prober() { return origin_prober_.get(); }

  PrefetchProxyOriginDecider* origin_decider() { return origin_decider_.get(); }

  // This call is forwarded to all |PrefetchProxySubresourceManager| in
  // |subresource_managers_| see documentation there for more detail.
  bool MaybeProxyURLLoaderFactory(
      content::RenderFrameHost* frame,
      int render_process_id,
      content::ContentBrowserClient::URLLoaderFactoryType type,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>*
          factory_receiver);

  // Creates an |PrefetchProxySubresourceManager| for the given |url|.
  PrefetchProxySubresourceManager* OnAboutToNoStatePrefetch(
      const GURL& url,
      std::unique_ptr<PrefetchedMainframeResponseContainer> response);

  // Returns a pointer to an |PrefetchProxySubresourceManager| for the given
  // URL, if one exists and hasn't been destroyed. Do not hold on to the
  // returned pointer since it may be deleted without notice.
  PrefetchProxySubresourceManager* GetSubresourceManagerForURL(
      const GURL& url) const;

  // Passes ownership of an |PrefetchProxySubresourceManager| for the given
  // URL, if one exists and hasn't been destroyed.
  std::unique_ptr<PrefetchProxySubresourceManager> TakeSubresourceManagerForURL(
      const GURL& url);

  // Destroys the subresource manager for the given url if one exists.
  void DestroySubresourceManagerForURL(const GURL& url);

  PrefetchProxyService(const PrefetchProxyService&) = delete;
  PrefetchProxyService& operator=(const PrefetchProxyService&) = delete;

 private:
  // Cleans up the NoStatePrerender response. Used in a delayed post task.
  void CleanupNoStatePrefetchResponse(const GURL& url);

  // The current profile, not owned.
  raw_ptr<Profile> profile_;

  // The custom proxy configurator for Prefetch Proxy.
  std::unique_ptr<PrefetchProxyProxyConfigurator> proxy_configurator_;

  // The origin prober class which manages all logic for origin probing.
  std::unique_ptr<PrefetchProxyOriginProber> origin_prober_;

  // The origin decider class which maintains persistent origin eligibility
  // logic.
  std::unique_ptr<PrefetchProxyOriginDecider> origin_decider_;

  // Map of prerender URL to its manager. Kept at the browser level since NSPs
  // are done in a separate WebContents from the one they are created in.
  std::map<GURL, std::unique_ptr<PrefetchProxySubresourceManager>>
      subresource_managers_;

  base::WeakPtrFactory<PrefetchProxyService> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PRELOADING_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_SERVICE_H_
