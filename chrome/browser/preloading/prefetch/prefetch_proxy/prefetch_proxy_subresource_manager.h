// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_SUBRESOURCE_MANAGER_H_
#define CHROME_BROWSER_PRELOADING_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_SUBRESOURCE_MANAGER_H_

#include <memory>
#include <set>

#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_proxying_url_loader_factory.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_handle.h"
#include "content/public/browser/content_browser_client.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/isolation_info.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
class RenderFrameHost;
}

namespace prerender {
class NoStatePrefetchHandle;
}

class PrefetchedMainframeResponseContainer;
class PrefetchProxyPrefetchMetricsCollector;
class PrefetchProxyProxyingURLLoaderFactory;

// This class manages the prefetch proxy of a page and its subresources.
class PrefetchProxySubresourceManager
    : public prerender::NoStatePrefetchHandle::Observer,
      public PrefetchProxyProxyingURLLoaderFactory::ResourceMetricsObserver {
 public:
  // A callback to create new URL Loader Factories for subresources.
  using CreateIsolatedLoaderFactoryRepeatingCallback =
      base::RepeatingCallback<void(
          mojo::PendingReceiver<network::mojom::URLLoaderFactory>
              pending_receiver,
          absl::optional<net::IsolationInfo> isolation_info)>;

  explicit PrefetchProxySubresourceManager(
      const GURL& url,
      std::unique_ptr<PrefetchedMainframeResponseContainer> mainframe_response);
  ~PrefetchProxySubresourceManager() override;

  // Passes ownership of |handle| to |this|, calling |on_nsp_done_callback| when
  // the NSP is done.
  void ManageNoStatePrefetch(
      std::unique_ptr<prerender::NoStatePrefetchHandle> handle,
      base::OnceClosure on_nsp_done_callback);

  bool has_nsp_handle() const { return !!nsp_handle_; }

  const std::set<GURL>& successfully_loaded_subresources() const {
    return successfully_loaded_subresources_;
  }

  // Sets the prefetch metrics collector to report subresource fetches to.
  void SetPrefetchMetricsCollector(
      scoped_refptr<PrefetchProxyPrefetchMetricsCollector> collector);

  // Takes ownership of |mainframe_response_|.
  std::unique_ptr<PrefetchedMainframeResponseContainer> TakeMainframeResponse();

  // Gives |this| a callback to create the isolated URL Loader factory to use
  // for Prefetch Proxy.
  void SetCreateIsolatedLoaderFactoryCallback(
      CreateIsolatedLoaderFactoryRepeatingCallback callback);

  // Called on commit to allow |this| to setup an intermediary (AKA: proxy, not
  // to be confused with a proxy server) URLLoaderFactory between the renderer
  // and network stack. Returns true when |factory_receiver| is proxied.
  bool MaybeProxyURLLoaderFactory(
      content::RenderFrameHost* frame,
      int render_process_id,
      content::ContentBrowserClient::URLLoaderFactoryType type,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>*
          factory_receiver);

  // Informs |this| that a navigation has taken place to this' |url_| and any
  // prefetched subresources should be loaded from cache.
  void NotifyPageNavigatedToAfterSRP();

  // Informs |this| that the origin probe to this' |url_| failed and no
  // prefetched subresources should be loaded from cache.
  void NotifyProbeFailed();

  // prerender::NoStatePrefetchHandle::Observer:
  void OnPrefetchStop(prerender::NoStatePrefetchHandle* handle) override;
  void OnPrefetchNetworkBytesChanged(
      prerender::NoStatePrefetchHandle* handle) override {}

  // PrefetchProxyProxyingURLLoaderFactory::ResourceMetricsObserver:
  void OnResourceFetchComplete(
      const GURL& url,
      network::mojom::URLResponseHeadPtr head,
      const network::URLLoaderCompletionStatus& status) override;
  void OnResourceNotEligible(const GURL& url,
                             PrefetchProxyPrefetchStatus status) override;
  void OnResourceThrottled(const GURL& url) override;
  void OnProxyUnavailableForResource(const GURL& url) override;
  void OnResourceUsedFromCache(const GURL& url) override;

  PrefetchProxySubresourceManager(const PrefetchProxySubresourceManager&) =
      delete;
  PrefetchProxySubresourceManager& operator=(
      const PrefetchProxySubresourceManager&) = delete;

 private:
  // Returns true if |MaybeProxyURLLoaderFactory| should proxy subresource loads
  // for a NSP.
  bool ShouldProxyForPrerenderNavigation(
      int render_process_id,
      content::ContentBrowserClient::URLLoaderFactoryType type);

  // Returns true if |MaybeProxyURLLoaderFactory| should proxy subresource loads
  // for a navigation was that previously prerendered.
  bool ShouldProxyForAfterSRPNavigation() const;

  // Called when the given factory is done serving all requests and can be
  // destroyed.
  void RemoveProxiedURLLoaderFactory(
      PrefetchProxyProxyingURLLoaderFactory* factory);

  // Used as
  // |PrefetchProxyProxyingURLLoaderFactory::ResourceLoadSuccessfulCallback|
  // to report a subresource was successfully loaded during the NSP.
  void OnSubresourceLoadSuccessful(const GURL& url);

  // The page that is being prerendered.
  const GURL url_;

  // Set in |NotifyPageNavigatedToAfterSRP| to signify that any subresource
  // loads in |successfully_loaded_subresources_| should be loaded from
  // |isolated_loader_factory_|'s cache but otherwise this is a normal page load
  // that can use the normal Profile's network context.
  bool was_navigated_to_after_srp_ = false;

  // All the subresources that have been successfully loaded during the NSP.
  // Each step in a subresource's redirect chain is also added here so that
  // those steps can be used from the network cache later as well.
  std::set<GURL> successfully_loaded_subresources_;

  // The mainframe response headers and body.
  std::unique_ptr<PrefetchedMainframeResponseContainer> mainframe_response_;

  // Collects metrics from the implementation of
  // |PrefetchProxyProxyingURLLoaderFactory::ResourceMetricsObserver|.
  scoped_refptr<PrefetchProxyPrefetchMetricsCollector> metrics_collector_;

  // State for managing the NoStatePrerender when it is running. If
  // |nsp_handle_| is set, then |on_nsp_done_callback_| is also set and vise
  // versa.
  std::unique_ptr<prerender::NoStatePrefetchHandle> nsp_handle_;
  base::OnceClosure on_nsp_done_callback_;

  // All owned proxying URL Loader Factories.
  std::set<std::unique_ptr<PrefetchProxyProxyingURLLoaderFactory>,
           base::UniquePtrComparator>
      proxied_loader_factories_;

  // A callback to create new URL Loader Factories for loading subresources.
  CreateIsolatedLoaderFactoryRepeatingCallback
      create_isolated_loader_factory_callback_;

  base::WeakPtrFactory<PrefetchProxySubresourceManager> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PRELOADING_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_SUBRESOURCE_MANAGER_H_
