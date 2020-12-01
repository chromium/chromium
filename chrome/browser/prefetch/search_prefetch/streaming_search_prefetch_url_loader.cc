// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/search_prefetch/streaming_search_prefetch_url_loader.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/storage_partition.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

StreamingSearchPrefetchURLLoader::StreamingSearchPrefetchURLLoader(
    StreamingSearchPrefetchRequest* streaming_prefetch_request,
    Profile* profile,
    std::unique_ptr<network::ResourceRequest> resource_request,
    const net::NetworkTrafficAnnotationTag& network_traffic_annotation)
    : resource_request_(std::move(resource_request)),
      streaming_prefetch_request_(streaming_prefetch_request) {
  DCHECK(streaming_prefetch_request_);
  auto url_loader_factory =
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetURLLoaderFactoryForBrowserProcess();

  // Create a network service URL loader with passed in params.
  url_loader_factory->CreateLoaderAndStart(
      network_url_loader_.BindNewPipeAndPassReceiver(), 0, 0,
      network::mojom::kURLLoadOptionNone, *resource_request_,
      url_loader_receiver_.BindNewPipeAndPassRemote(
          base::ThreadTaskRunnerHandle::Get()),
      net::MutableNetworkTrafficAnnotationTag(network_traffic_annotation));
  url_loader_receiver_.set_disconnect_handler(
      base::BindOnce(&StreamingSearchPrefetchURLLoader::OnMojoDisconnect,
                     base::Unretained(this)));
}

StreamingSearchPrefetchURLLoader::~StreamingSearchPrefetchURLLoader() = default;

SearchPrefetchURLLoader::RequestHandler
StreamingSearchPrefetchURLLoader::ServingResponseHandler() {
  DCHECK(!streaming_prefetch_request_);
  return base::BindOnce(
      &StreamingSearchPrefetchURLLoader::SetUpForwardingClient,
      weak_factory_.GetWeakPtr());
}

void StreamingSearchPrefetchURLLoader::SetUpForwardingClient(
    const network::ResourceRequest& resource_request,
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> forwarding_client) {
  DCHECK(!streaming_prefetch_request_);
  // Bind to the content/ navigation code.
  DCHECK(!receiver_.is_bound());
  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(
      base::BindOnce(&StreamingSearchPrefetchURLLoader::OnMojoDisconnect,
                     weak_factory_.GetWeakPtr()));
  forwarding_client_.Bind(std::move(forwarding_client));

  if (!resource_request.report_raw_headers) {
    resource_response_->raw_request_response_info = nullptr;
  }

  forwarding_client_->OnReceiveResponse(std::move(resource_response_));

  // Resume previously paused network service URLLoader.
  url_loader_receiver_.Resume();
}

void StreamingSearchPrefetchURLLoader::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr head) {
  DCHECK(!forwarding_client_);
  DCHECK(streaming_prefetch_request_);

  // Store head and pause new messages until the forwarding client is set up.
  resource_response_ = std::move(head);

  if (!streaming_prefetch_request_->CanServePrefetchRequest(
          resource_response_->headers)) {
    // Not safe to do anything after this point
    streaming_prefetch_request_->ErrorEncountered();
    return;
  }

  streaming_prefetch_request_->MarkPrefetchAsServable();
  url_loader_receiver_.Pause();
}

void StreamingSearchPrefetchURLLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr head) {
  if (streaming_prefetch_request_) {
    streaming_prefetch_request_->ErrorEncountered();
  } else {
    delete this;
  }
}

void StreamingSearchPrefetchURLLoader::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback callback) {
  // We only handle GETs.
  NOTREACHED();
}

void StreamingSearchPrefetchURLLoader::OnReceiveCachedMetadata(
    mojo_base::BigBuffer data) {
  // Do nothing. This is not supported for navigation loader.
}

void StreamingSearchPrefetchURLLoader::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {
  DCHECK(forwarding_client_);
  forwarding_client_->OnTransferSizeUpdated(transfer_size_diff);
}

void StreamingSearchPrefetchURLLoader::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  DCHECK(forwarding_client_);
  DCHECK(!streaming_prefetch_request_);
  forwarding_client_->OnStartLoadingResponseBody(std::move(body));
}

void StreamingSearchPrefetchURLLoader::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  DCHECK(!streaming_prefetch_request_);
  if (forwarding_client_) {
    forwarding_client_->OnComplete(status);
    return;
  }

  NOTREACHED();
}

void StreamingSearchPrefetchURLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const base::Optional<GURL>& new_url) {
  // This should never be called for a non-network service URLLoader.
  NOTREACHED();
}

void StreamingSearchPrefetchURLLoader::SetPriority(
    net::RequestPriority priority,
    int32_t intra_priority_value) {
  // Pass through.
  network_url_loader_->SetPriority(priority, intra_priority_value);
}

void StreamingSearchPrefetchURLLoader::PauseReadingBodyFromNet() {
  // Pass through.
  network_url_loader_->PauseReadingBodyFromNet();
}

void StreamingSearchPrefetchURLLoader::ResumeReadingBodyFromNet() {
  // Pass through.
  network_url_loader_->ResumeReadingBodyFromNet();
}

void StreamingSearchPrefetchURLLoader::OnMojoDisconnect() {
  if (streaming_prefetch_request_) {
    streaming_prefetch_request_->ErrorEncountered();
  } else {
    delete this;
  }
}

void StreamingSearchPrefetchURLLoader::ClearOwnerPointer() {
  streaming_prefetch_request_ = nullptr;
}
