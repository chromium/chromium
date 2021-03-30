// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/search_prefetch/back_forward_search_prefetch_url_loader.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/prefetch/search_prefetch/search_prefetch_service.h"
#include "chrome/browser/prefetch/search_prefetch/search_prefetch_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

BackForwardSearchPrefetchURLLoader::BackForwardSearchPrefetchURLLoader(
    Profile* profile,
    const net::NetworkTrafficAnnotationTag& network_traffic_annotation,
    const GURL& prefetch_url)
    : profile_(profile),
      network_traffic_annotation_(network_traffic_annotation),
      prefetch_url_(prefetch_url) {}

BackForwardSearchPrefetchURLLoader::~BackForwardSearchPrefetchURLLoader() =
    default;

SearchPrefetchURLLoader::RequestHandler
BackForwardSearchPrefetchURLLoader::ServingResponseHandler(
    std::unique_ptr<SearchPrefetchURLLoader> loader) {
  return base::BindOnce(
      &BackForwardSearchPrefetchURLLoader::SetUpForwardingClient,
      weak_factory_.GetWeakPtr(), std::move(loader));
}

void BackForwardSearchPrefetchURLLoader::SetUpForwardingClient(
    std::unique_ptr<SearchPrefetchURLLoader> loader,
    const network::ResourceRequest& resource_request,
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> forwarding_client) {
  resource_request_ =
      std::make_unique<network::ResourceRequest>(resource_request);

  network::ResourceRequest prefetch_request = *resource_request_;

  prefetch_request.load_flags |= net::LOAD_ONLY_FROM_CACHE;
  prefetch_request.url = prefetch_url_;

  auto url_loader_factory =
      content::BrowserContext::GetDefaultStoragePartition(profile_)
          ->GetURLLoaderFactoryForBrowserProcess();

  // Create a network service URL loader with passed in params.
  url_loader_factory->CreateLoaderAndStart(
      network_url_loader_.BindNewPipeAndPassReceiver(), 0,
      network::mojom::kURLLoadOptionNone, prefetch_request,
      url_loader_receiver_.BindNewPipeAndPassRemote(
          base::ThreadTaskRunnerHandle::Get()),
      net::MutableNetworkTrafficAnnotationTag(network_traffic_annotation_));
  url_loader_receiver_.set_disconnect_handler(base::BindOnce(
      &BackForwardSearchPrefetchURLLoader::MojoDisconnectForPrefetch,
      base::Unretained(this)));

  // Bind to the content/ navigation code.
  DCHECK(!receiver_.is_bound());

  // At this point, we are bound to the mojo receiver, so we can release
  // |loader|, which points to |this|.
  receiver_.Bind(std::move(receiver));
  loader.release();
  receiver_.set_disconnect_handler(base::BindOnce(
      &BackForwardSearchPrefetchURLLoader::MojoDisconnectWithNoFallback,
      weak_factory_.GetWeakPtr()));
  forwarding_client_.Bind(std::move(forwarding_client));
}

void BackForwardSearchPrefetchURLLoader::RestartDirect() {
  network_url_loader_.reset();
  url_loader_receiver_.reset();
  can_fallback_ = false;

  SearchPrefetchService* service =
      SearchPrefetchServiceFactory::GetForProfile(profile_);
  if (service)
    service->ClearCacheEntry(resource_request_->url);

  auto url_loader_factory =
      content::BrowserContext::GetDefaultStoragePartition(profile_)
          ->GetURLLoaderFactoryForBrowserProcess();

  // Create a network service URL loader with passed in params.
  url_loader_factory->CreateLoaderAndStart(
      network_url_loader_.BindNewPipeAndPassReceiver(), 0,
      network::mojom::kURLLoadOptionNone, *resource_request_,
      url_loader_receiver_.BindNewPipeAndPassRemote(
          base::ThreadTaskRunnerHandle::Get()),
      net::MutableNetworkTrafficAnnotationTag(network_traffic_annotation_));
  url_loader_receiver_.set_disconnect_handler(base::BindOnce(
      &BackForwardSearchPrefetchURLLoader::MojoDisconnectWithNoFallback,
      base::Unretained(this)));
  if (paused_) {
    network_url_loader_->PauseReadingBodyFromNet();
  }
}

void BackForwardSearchPrefetchURLLoader::OnReceiveEarlyHints(
    network::mojom::EarlyHintsPtr early_hints) {
  DCHECK(forwarding_client_);
  forwarding_client_->OnReceiveEarlyHints(std::move(early_hints));
}

void BackForwardSearchPrefetchURLLoader::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr head) {
  DCHECK(forwarding_client_);
  if (can_fallback_) {
    if (!head->headers) {
      RestartDirect();
      return;
    }

    // Any 200 response can be served.
    if (head->headers->response_code() < net::HTTP_OK ||
        head->headers->response_code() >= net::HTTP_MULTIPLE_CHOICES) {
      RestartDirect();
      return;
    }
    url_loader_receiver_.set_disconnect_handler(base::BindOnce(
        &BackForwardSearchPrefetchURLLoader::MojoDisconnectWithNoFallback,
        weak_factory_.GetWeakPtr()));

    SearchPrefetchService* service =
        SearchPrefetchServiceFactory::GetForProfile(profile_);
    if (service)
      service->UpdateServeTime(resource_request_->url);
  }

  can_fallback_ = false;
  forwarding_client_->OnReceiveResponse(std::move(head));
}

void BackForwardSearchPrefetchURLLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr head) {
  DCHECK(forwarding_client_);
  if (can_fallback_) {
    RestartDirect();
    return;
  }

  forwarding_client_->OnReceiveRedirect(redirect_info, std::move(head));
}

void BackForwardSearchPrefetchURLLoader::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback callback) {
  // We only handle GETs.
  NOTREACHED();
}

void BackForwardSearchPrefetchURLLoader::OnReceiveCachedMetadata(
    mojo_base::BigBuffer data) {
  // Do nothing. This is not supported for navigation loader.
}

void BackForwardSearchPrefetchURLLoader::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {
  DCHECK(forwarding_client_);
  forwarding_client_->OnTransferSizeUpdated(transfer_size_diff);
}

void BackForwardSearchPrefetchURLLoader::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  DCHECK(forwarding_client_);
  forwarding_client_->OnStartLoadingResponseBody(std::move(body));
  return;
}

void BackForwardSearchPrefetchURLLoader::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  if (status.error_code != net::OK && can_fallback_) {
    RestartDirect();
    return;
  }
  DCHECK(forwarding_client_);
  can_fallback_ = false;
  forwarding_client_->OnComplete(status);
  network_url_loader_.reset();
}

void BackForwardSearchPrefetchURLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const base::Optional<GURL>& new_url) {
  // This should never be called for a non-network service URLLoader.
  NOTREACHED();
}

void BackForwardSearchPrefetchURLLoader::SetPriority(
    net::RequestPriority priority,
    int32_t intra_priority_value) {
  // Pass through.
  if (network_url_loader_)
    network_url_loader_->SetPriority(priority, intra_priority_value);

  resource_request_->priority = priority;
}

void BackForwardSearchPrefetchURLLoader::PauseReadingBodyFromNet() {
  // Pass through.
  if (network_url_loader_)
    network_url_loader_->PauseReadingBodyFromNet();
  paused_ = true;
}

void BackForwardSearchPrefetchURLLoader::ResumeReadingBodyFromNet() {
  // Pass through.
  if (network_url_loader_)
    network_url_loader_->ResumeReadingBodyFromNet();
  paused_ = false;
}

void BackForwardSearchPrefetchURLLoader::MojoDisconnectForPrefetch() {
  if (can_fallback_)
    RestartDirect();
}

void BackForwardSearchPrefetchURLLoader::MojoDisconnectWithNoFallback() {
  delete this;
}
