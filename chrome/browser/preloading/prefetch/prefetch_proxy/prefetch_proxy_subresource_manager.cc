// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_subresource_manager.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_prefetch_metrics_collector.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetched_mainframe_response_container.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

PrefetchProxySubresourceManager::PrefetchProxySubresourceManager(
    const GURL& url,
    std::unique_ptr<PrefetchedMainframeResponseContainer> mainframe_response)
    : url_(url), mainframe_response_(std::move(mainframe_response)) {}

PrefetchProxySubresourceManager::~PrefetchProxySubresourceManager() {
  if (nsp_handle_) {
    nsp_handle_->SetObserver(nullptr);
    nsp_handle_->OnCancel();
  }
}

void PrefetchProxySubresourceManager::ManageNoStatePrefetch(
    std::unique_ptr<prerender::NoStatePrefetchHandle> handle,
    base::OnceClosure on_nsp_done_callback) {
  on_nsp_done_callback_ = std::move(on_nsp_done_callback);
  nsp_handle_ = std::move(handle);
  nsp_handle_->SetObserver(this);
}

std::unique_ptr<PrefetchedMainframeResponseContainer>
PrefetchProxySubresourceManager::TakeMainframeResponse() {
  return std::move(mainframe_response_);
}

void PrefetchProxySubresourceManager::SetCreateIsolatedLoaderFactoryCallback(
    CreateIsolatedLoaderFactoryRepeatingCallback callback) {
  create_isolated_loader_factory_callback_ = std::move(callback);
}

void PrefetchProxySubresourceManager::NotifyPageNavigatedToAfterSRP() {
  DCHECK(create_isolated_loader_factory_callback_);
  // We're navigating so take the extra work off the CPU.
  if (nsp_handle_) {
    OnPrefetchStop(nsp_handle_.get());
  }

  was_navigated_to_after_srp_ = true;
}

void PrefetchProxySubresourceManager::OnPrefetchStop(
    prerender::NoStatePrefetchHandle* handle) {
  DCHECK_EQ(nsp_handle_.get(), handle);

  if (on_nsp_done_callback_) {
    std::move(on_nsp_done_callback_).Run();
  }

  // The handle must be canceled before it can be destroyed.
  nsp_handle_->OnCancel();
  nsp_handle_.reset();
}

bool PrefetchProxySubresourceManager::ShouldProxyForPrerenderNavigation(
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
      nsp_handle_->contents()->no_state_prefetch_contents();
  if (!web_contents) {
    // This shouldn't happen, so abort the prerender just to be safe.
    OnPrefetchStop(nsp_handle_.get());
    NOTREACHED();
    return false;
  }

  int prerender_process_id =
      web_contents->GetPrimaryMainFrame()->GetProcess()->GetID();
  if (prerender_process_id != render_process_id) {
    return false;
  }

  if (!create_isolated_loader_factory_callback_) {
    // This also shouldn't happen, and would imply that there is a bug in the
    // code where a prerender was triggered without having an isolated URL
    // Loader Factory callback to use. Abort the prerender just to be safe.
    OnPrefetchStop(nsp_handle_.get());
    NOTREACHED();
    return false;
  }

  return true;
}

bool PrefetchProxySubresourceManager::ShouldProxyForAfterSRPNavigation() const {
  return was_navigated_to_after_srp_;
}

bool PrefetchProxySubresourceManager::MaybeProxyURLLoaderFactory(
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
      frame->GetPendingIsolationInfoForSubresources());

  auto proxy = std::make_unique<PrefetchProxyProxyingURLLoaderFactory>(
      this, frame->GetFrameTreeNodeId(), std::move(proxied_receiver),
      std::move(network_process_factory_remote),
      std::move(isolated_factory_remote),
      base::BindOnce(
          &PrefetchProxySubresourceManager::RemoveProxiedURLLoaderFactory,
          weak_factory_.GetWeakPtr()),
      base::BindRepeating(
          &PrefetchProxySubresourceManager::OnSubresourceLoadSuccessful,
          weak_factory_.GetWeakPtr()));

  if (ShouldProxyForAfterSRPNavigation()) {
    proxy->NotifyPageNavigatedToAfterSRP(successfully_loaded_subresources_);
  }

  proxied_loader_factories_.emplace(std::move(proxy));

  return true;
}

void PrefetchProxySubresourceManager::OnSubresourceLoadSuccessful(
    const GURL& url) {
  successfully_loaded_subresources_.emplace(url);
}

void PrefetchProxySubresourceManager::NotifyProbeFailed() {
  successfully_loaded_subresources_.clear();
}

void PrefetchProxySubresourceManager::RemoveProxiedURLLoaderFactory(
    PrefetchProxyProxyingURLLoaderFactory* factory) {
  auto it = proxied_loader_factories_.find(factory);
  DCHECK(it != proxied_loader_factories_.end());
  proxied_loader_factories_.erase(it);
}

void PrefetchProxySubresourceManager::SetPrefetchMetricsCollector(
    scoped_refptr<PrefetchProxyPrefetchMetricsCollector> collector) {
  metrics_collector_ = collector;
}

void PrefetchProxySubresourceManager::OnResourceFetchComplete(
    const GURL& url,
    network::mojom::URLResponseHeadPtr head,
    const network::URLLoaderCompletionStatus& status) {
  if (!metrics_collector_)
    return;

  metrics_collector_->OnSubresourcePrefetched(
      /*mainframe_url=*/url_,
      /*subresource_url=*/url, std::move(head), status);
}

void PrefetchProxySubresourceManager::OnResourceNotEligible(
    const GURL& url,
    PrefetchProxyPrefetchStatus status) {
  if (!metrics_collector_)
    return;
  metrics_collector_->OnSubresourceNotEligible(
      /*mainframe_url=*/url_,
      /*subresource_url=*/url, status);
}

void PrefetchProxySubresourceManager::OnResourceThrottled(const GURL& url) {
  if (!metrics_collector_)
    return;
  metrics_collector_->OnSubresourceNotEligible(
      /*mainframe_url=*/url_,
      /*subresource_url=*/url,
      PrefetchProxyPrefetchStatus::kSubresourceThrottled);
}

void PrefetchProxySubresourceManager::OnProxyUnavailableForResource(
    const GURL& url) {
  if (!metrics_collector_)
    return;
  metrics_collector_->OnSubresourceNotEligible(
      /*mainframe_url=*/url_,
      /*subresource_url=*/url,
      PrefetchProxyPrefetchStatus::kPrefetchProxyNotAvailable);
}

void PrefetchProxySubresourceManager::OnResourceUsedFromCache(const GURL& url) {
  if (!metrics_collector_)
    return;
  metrics_collector_->OnCachedSubresourceUsed(/*mainframe_url=*/url_,
                                              /*subresource_url=*/url);
}
