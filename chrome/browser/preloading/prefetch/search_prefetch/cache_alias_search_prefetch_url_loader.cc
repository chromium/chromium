// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/search_prefetch/cache_alias_search_prefetch_url_loader.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/record_ontransfersizeupdate_utils.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

CacheAliasSearchPrefetchURLLoader::CacheAliasSearchPrefetchURLLoader(
    Profile* profile,
    const net::NetworkTrafficAnnotationTag& network_traffic_annotation,
    const GURL& prefetch_url)
    : url_loader_factory_(profile->GetDefaultStoragePartition()
                              ->GetURLLoaderFactoryForBrowserProcess()),
      search_prefetch_service_(
          SearchPrefetchServiceFactory::GetForProfile(profile)->GetWeakPtr()),
      network_traffic_annotation_(network_traffic_annotation),
      prefetch_url_(prefetch_url) {}

CacheAliasSearchPrefetchURLLoader::~CacheAliasSearchPrefetchURLLoader() {
  base::UmaHistogramEnumeration(
      "Omnibox.SearchPrefetch.CacheAliasFallbackReason", fallback_reason_);
}

// static
SearchPrefetchURLLoader::RequestHandler
CacheAliasSearchPrefetchURLLoader::GetServingResponseHandlerFromLoader(
    std::unique_ptr<CacheAliasSearchPrefetchURLLoader> loader) {
  DCHECK(loader);
  loader->RecordInterceptionTime();
  base::WeakPtr<CacheAliasSearchPrefetchURLLoader> weak_ptr_loader =
      loader->weak_factory_.GetWeakPtr();
  return base::BindOnce(
      &CacheAliasSearchPrefetchURLLoader::SetUpForwardingClient,
      std::move(weak_ptr_loader), std::move(loader));
}

void CacheAliasSearchPrefetchURLLoader::SetUpForwardingClient(
    std::unique_ptr<SearchPrefetchURLLoader> loader,
    const network::ResourceRequest& resource_request,
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> forwarding_client) {
  resource_request_ =
      std::make_unique<network::ResourceRequest>(resource_request);

  // Bind to the content/ navigation code.
  DCHECK(!receiver_.is_bound());

  // At this point, we are bound to the mojo receiver, so we can release
  // |loader|, which points to |this|.
  receiver_.Bind(std::move(receiver));
  loader.release();
  receiver_.set_disconnect_handler(base::BindOnce(
      &CacheAliasSearchPrefetchURLLoader::MojoDisconnectWithNoFallback,
      weak_factory_.GetWeakPtr()));
  forwarding_client_.Bind(std::move(forwarding_client));

  StartPrefetchRequest();
}

void CacheAliasSearchPrefetchURLLoader::StartPrefetchRequest() {
  network::ResourceRequest prefetch_request = *resource_request_;

  prefetch_request.load_flags |= net::LOAD_ONLY_FROM_CACHE;
  prefetch_request.url = prefetch_url_;

  // Create a network service URL loader with passed in params.
  url_loader_factory_->CreateLoaderAndStart(
      network_url_loader_.BindNewPipeAndPassReceiver(), 0,
      network::mojom::kURLLoadOptionNone, prefetch_request,
      url_loader_receiver_.BindNewPipeAndPassRemote(
          base::SingleThreadTaskRunner::GetCurrentDefault()),
      net::MutableNetworkTrafficAnnotationTag(network_traffic_annotation_));
  url_loader_receiver_.set_disconnect_handler(base::BindOnce(
      &CacheAliasSearchPrefetchURLLoader::MojoDisconnectForPrefetch,
      base::Unretained(this)));
}

void CacheAliasSearchPrefetchURLLoader::RestartDirect(
    FallbackReason fallback_reason) {
  CHECK(can_fallback_);
  can_fallback_ = false;

  network_url_loader_.reset();
  url_loader_receiver_.reset();

  CHECK_EQ(fallback_reason_, FallbackReason::kNoFallback);
  CHECK_NE(fallback_reason, FallbackReason::kNoFallback);
  fallback_reason_ = fallback_reason;

  if (search_prefetch_service_) {
    search_prefetch_service_->ClearCacheEntry(resource_request_->url);
  }

  base::UmaHistogramTimes(
      "Omnibox.SearchPrefetch.CacheAliasElapsedTimeToFallback",
      timer_from_ctor_.Elapsed());

  // Create a network service URL loader with passed in params.
  url_loader_factory_->CreateLoaderAndStart(
      network_url_loader_.BindNewPipeAndPassReceiver(), 0,
      network::mojom::kURLLoadOptionSendSSLInfoWithResponse |
          network::mojom::kURLLoadOptionSniffMimeType |
          network::mojom::kURLLoadOptionSendSSLInfoForCertificateError,
      *resource_request_,
      url_loader_receiver_.BindNewPipeAndPassRemote(
          base::SingleThreadTaskRunner::GetCurrentDefault()),
      net::MutableNetworkTrafficAnnotationTag(network_traffic_annotation_));
  url_loader_receiver_.set_disconnect_handler(base::BindOnce(
      &CacheAliasSearchPrefetchURLLoader::MojoDisconnectWithNoFallback,
      base::Unretained(this)));
  if (paused_) {
    network_url_loader_->PauseReadingBodyFromNet();
  }
}

void CacheAliasSearchPrefetchURLLoader::OnReceiveEarlyHints(
    network::mojom::EarlyHintsPtr early_hints) {
  DCHECK(forwarding_client_);
  forwarding_client_->OnReceiveEarlyHints(std::move(early_hints));
}

void CacheAliasSearchPrefetchURLLoader::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr head,
    mojo::ScopedDataPipeConsumerHandle body,
    std::optional<mojo_base::BigBuffer> cached_metadata) {
  DCHECK(forwarding_client_);
  if (can_fallback_) {
    if (!head->headers) {
      RestartDirect(FallbackReason::kNoResponseHeaders);
      return;
    }

    // Any 200 response can be served.
    if (head->headers->response_code() < net::HTTP_OK ||
        head->headers->response_code() >= net::HTTP_MULTIPLE_CHOICES) {
      RestartDirect(FallbackReason::kNon2xxResponse);
      return;
    }
    url_loader_receiver_.set_disconnect_handler(base::BindOnce(
        &CacheAliasSearchPrefetchURLLoader::MojoDisconnectWithNoFallback,
        weak_factory_.GetWeakPtr()));

    if (search_prefetch_service_) {
      search_prefetch_service_->UpdateServeTime(resource_request_->url);
    }
  }

  // Cached metadata is not supported for navigation loader.
  cached_metadata.reset();

  can_fallback_ = false;
  forwarding_client_->OnReceiveResponse(std::move(head), std::move(body),
                                        std::nullopt);
}

void CacheAliasSearchPrefetchURLLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr head) {
  DCHECK(forwarding_client_);
  if (can_fallback_) {
    RestartDirect(FallbackReason::kRedirectResponse);
    return;
  }

  forwarding_client_->OnReceiveRedirect(redirect_info, std::move(head));
}

void CacheAliasSearchPrefetchURLLoader::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback callback) {
  // We only handle GETs.
  NOTREACHED_IN_MIGRATION();
}

void CacheAliasSearchPrefetchURLLoader::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {
  DCHECK(forwarding_client_);
  network::RecordOnTransferSizeUpdatedUMA(
      network::OnTransferSizeUpdatedFrom::kCacheAliasSearchPrefetchURLLoader);
  forwarding_client_->OnTransferSizeUpdated(transfer_size_diff);
}

void CacheAliasSearchPrefetchURLLoader::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  if (status.error_code != net::OK && can_fallback_) {
    RestartDirect(FallbackReason::kErrorOnComplete);
    return;
  }
  DCHECK(forwarding_client_);
  can_fallback_ = false;
  forwarding_client_->OnComplete(status);
  OnForwardingComplete();
  network_url_loader_.reset();
}

void CacheAliasSearchPrefetchURLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const std::optional<GURL>& new_url) {
  // This should never be called for a non-network service URLLoader.
  NOTREACHED_IN_MIGRATION();
}

void CacheAliasSearchPrefetchURLLoader::SetPriority(
    net::RequestPriority priority,
    int32_t intra_priority_value) {
  // Pass through.
  if (network_url_loader_) {
    network_url_loader_->SetPriority(priority, intra_priority_value);
  }

  resource_request_->priority = priority;
}

void CacheAliasSearchPrefetchURLLoader::PauseReadingBodyFromNet() {
  // Pass through.
  if (network_url_loader_) {
    network_url_loader_->PauseReadingBodyFromNet();
  }
  paused_ = true;
}

void CacheAliasSearchPrefetchURLLoader::ResumeReadingBodyFromNet() {
  // Pass through.
  if (network_url_loader_) {
    network_url_loader_->ResumeReadingBodyFromNet();
  }
  paused_ = false;
}

void CacheAliasSearchPrefetchURLLoader::MojoDisconnectForPrefetch() {
  if (can_fallback_) {
    RestartDirect(FallbackReason::kMojoDisconnect);
  }
}

void CacheAliasSearchPrefetchURLLoader::MojoDisconnectWithNoFallback() {
  delete this;
}
