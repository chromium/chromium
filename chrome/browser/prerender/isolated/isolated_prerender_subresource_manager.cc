// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/isolated/isolated_prerender_subresource_manager.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_prefetch_metrics_collector.h"
#include "chrome/browser/prerender/isolated/prefetched_mainframe_response_container.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

IsolatedPrerenderSubresourceManager::IsolatedPrerenderSubresourceManager(
    const GURL& url,
    std::unique_ptr<PrefetchedMainframeResponseContainer> mainframe_response)
    : url_(url), mainframe_response_(std::move(mainframe_response)) {}

IsolatedPrerenderSubresourceManager::~IsolatedPrerenderSubresourceManager() {
  if (nsp_handle_) {
    nsp_handle_->SetObserver(nullptr);
    nsp_handle_->OnCancel();
  }
  UMA_HISTOGRAM_COUNTS_100("IsolatedPrerender.Prefetch.Subresources.Quantity",
                           successfully_loaded_subresources_.size());
}

void IsolatedPrerenderSubresourceManager::ManageNoStatePrefetch(
    std::unique_ptr<prerender::PrerenderHandle> handle,
    base::OnceClosure on_nsp_done_callback) {
  on_nsp_done_callback_ = std::move(on_nsp_done_callback);
  nsp_handle_ = std::move(handle);
  nsp_handle_->SetObserver(this);
}

std::unique_ptr<PrefetchedMainframeResponseContainer>
IsolatedPrerenderSubresourceManager::TakeMainframeResponse() {
  return std::move(mainframe_response_);
}

void IsolatedPrerenderSubresourceManager::
    SetCreateIsolatedLoaderFactoryCallback(
        CreateIsolatedLoaderFactoryRepeatingCallback callback) {
  create_isolated_loader_factory_callback_ = std::move(callback);
}

void IsolatedPrerenderSubresourceManager::NotifyPageNavigatedToAfterSRP() {
  DCHECK(create_isolated_loader_factory_callback_);
  // We're navigating so take the extra work off the CPU.
  if (nsp_handle_) {
    OnPrerenderStop(nsp_handle_.get());
  }

  was_navigated_to_after_srp_ = true;
}

void IsolatedPrerenderSubresourceManager::OnPrerenderStop(
    prerender::PrerenderHandle* handle) {
  DCHECK_EQ(nsp_handle_.get(), handle);

  if (on_nsp_done_callback_) {
    std::move(on_nsp_done_callback_).Run();
  }

  // The handle must be canceled before it can be destroyed.
  nsp_handle_->OnCancel();
  nsp_handle_.reset();
}

bool IsolatedPrerenderSubresourceManager::ShouldProxyForPrerenderNavigation(
    int render_process_id,
    content::ContentBrowserClient::URLLoaderFactoryType type) {
  if (type != content::ContentBrowserClient::URLLoaderFactoryType::
                  kDocumentSubResource) {
    return false;
  }

  if (!nsp_handle_) {
    return false;
  }

  content::WebContents* web_contents =
      nsp_handle_->contents()->prerender_contents();
  if (!web_contents) {
    // This shouldn't happen, so abort the prerender just to be safe.
    OnPrerenderStop(nsp_handle_.get());
    NOTREACHED();
    return false;
  }

  int prerender_process_id =
      web_contents->GetMainFrame()->GetProcess()->GetID();
  if (prerender_process_id != render_process_id) {
    return false;
  }

  if (!create_isolated_loader_factory_callback_) {
    // This also shouldn't happen, and would imply that there is a bug in the
    // code where a prerender was triggered without having an isolated URL
    // Loader Factory callback to use. Abort the prerender just to be safe.
    OnPrerenderStop(nsp_handle_.get());
    NOTREACHED();
    return false;
  }

  return true;
}

bool IsolatedPrerenderSubresourceManager::ShouldProxyForAfterSRPNavigation()
    const {
  return was_navigated_to_after_srp_;
}

bool IsolatedPrerenderSubresourceManager::MaybeProxyURLLoaderFactory(
    content::RenderFrameHost* frame,
    int render_process_id,
    content::ContentBrowserClient::URLLoaderFactoryType type,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory>* factory_receiver) {
  if (!ShouldProxyForPrerenderNavigation(render_process_id, type) &&
      !ShouldProxyForAfterSRPNavigation()) {
    return false;
  }

  auto proxied_receiver = std::move(*factory_receiver);
  mojo::PendingRemote<network::mojom::URLLoaderFactory>
      network_process_factory_remote;
  *factory_receiver =
      network_process_factory_remote.InitWithNewPipeAndPassReceiver();

  mojo::PendingRemote<network::mojom::URLLoaderFactory> isolated_factory_remote;
  create_isolated_loader_factory_callback_.Run(
      isolated_factory_remote.InitWithNewPipeAndPassReceiver(),
      frame->GetIsolationInfoForSubresources());

  auto proxy = std::make_unique<IsolatedPrerenderProxyingURLLoaderFactory>(
      this, frame->GetFrameTreeNodeId(), std::move(proxied_receiver),
      std::move(network_process_factory_remote),
      std::move(isolated_factory_remote),
      base::BindOnce(
          &IsolatedPrerenderSubresourceManager::RemoveProxiedURLLoaderFactory,
          weak_factory_.GetWeakPtr()),
      base::BindRepeating(
          &IsolatedPrerenderSubresourceManager::OnSubresourceLoadSuccessful,
          weak_factory_.GetWeakPtr()));

  if (ShouldProxyForAfterSRPNavigation()) {
    proxy->NotifyPageNavigatedToAfterSRP(successfully_loaded_subresources_);
  }

  proxied_loader_factories_.emplace(std::move(proxy));

  return true;
}

void IsolatedPrerenderSubresourceManager::OnSubresourceLoadSuccessful(
    const GURL& url) {
  successfully_loaded_subresources_.emplace(url);
}

void IsolatedPrerenderSubresourceManager::NotifyProbeFailed() {
  successfully_loaded_subresources_.clear();
}

void IsolatedPrerenderSubresourceManager::RemoveProxiedURLLoaderFactory(
    IsolatedPrerenderProxyingURLLoaderFactory* factory) {
  auto it = proxied_loader_factories_.find(factory);
  DCHECK(it != proxied_loader_factories_.end());
  proxied_loader_factories_.erase(it);
}

void IsolatedPrerenderSubresourceManager::SetPrefetchMetricsCollector(
    scoped_refptr<IsolatedPrerenderPrefetchMetricsCollector> collector) {
  metrics_collector_ = collector;
}

void IsolatedPrerenderSubresourceManager::OnResourceFetchComplete(
    const GURL& url,
    network::mojom::URLResponseHeadPtr head,
    const network::URLLoaderCompletionStatus& status) {
  if (!metrics_collector_)
    return;

  metrics_collector_->OnSubresourcePrefetched(
      /*mainframe_url=*/url_,
      /*subresource_url=*/url, std::move(head), status);
}

void IsolatedPrerenderSubresourceManager::OnResourceNotEligible(
    const GURL& url,
    IsolatedPrerenderPrefetchStatus status) {
  if (!metrics_collector_)
    return;
  metrics_collector_->OnSubresourceNotEligible(
      /*mainframe_url=*/url_,
      /*subresource_url=*/url, status);
}

void IsolatedPrerenderSubresourceManager::OnResourceThrottled(const GURL& url) {
  if (!metrics_collector_)
    return;
  metrics_collector_->OnSubresourceNotEligible(
      /*mainframe_url=*/url_,
      /*subresource_url=*/url,
      IsolatedPrerenderPrefetchStatus::kSubresourceThrottled);
}

void IsolatedPrerenderSubresourceManager::OnResourceUsedFromCache(
    const GURL& url) {
  if (!metrics_collector_)
    return;
  metrics_collector_->OnCachedSubresourceUsed(/*mainframe_url=*/url_,
                                              /*subresource_url=*/url);
}
