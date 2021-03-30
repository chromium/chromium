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
#include "mojo/public/c/system/data_pipe.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/constants.h"
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
      network_url_loader_.BindNewPipeAndPassReceiver(), 0,
      network::mojom::kURLLoadOptionNone, *resource_request_,
      url_loader_receiver_.BindNewPipeAndPassRemote(
          base::ThreadTaskRunnerHandle::Get()),
      net::MutableNetworkTrafficAnnotationTag(network_traffic_annotation));
  url_loader_receiver_.set_disconnect_handler(base::BindOnce(
      &StreamingSearchPrefetchURLLoader::OnURLLoaderMojoDisconnect,
      base::Unretained(this)));
}

StreamingSearchPrefetchURLLoader::~StreamingSearchPrefetchURLLoader() = default;

SearchPrefetchURLLoader::RequestHandler
StreamingSearchPrefetchURLLoader::ServingResponseHandler(
    std::unique_ptr<SearchPrefetchURLLoader> loader) {
  DCHECK(!streaming_prefetch_request_);
  return base::BindOnce(
      &StreamingSearchPrefetchURLLoader::SetUpForwardingClient,
      weak_factory_.GetWeakPtr(), std::move(loader));
}

void StreamingSearchPrefetchURLLoader::SetUpForwardingClient(
    std::unique_ptr<SearchPrefetchURLLoader> loader,
    const network::ResourceRequest& resource_request,
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> forwarding_client) {
  DCHECK(!streaming_prefetch_request_);
  // Bind to the content/ navigation code.
  DCHECK(!receiver_.is_bound());
  if (network_url_loader_)
    network_url_loader_->SetPriority(resource_request.priority, -1);

  // At this point, we are bound to the mojo receiver, so we can release
  // |loader|, which points to |this|.
  receiver_.Bind(std::move(receiver));
  loader.release();
  receiver_.set_disconnect_handler(base::BindOnce(
      &StreamingSearchPrefetchURLLoader::OnURLLoaderClientMojoDisconnect,
      weak_factory_.GetWeakPtr()));
  forwarding_client_.Bind(std::move(forwarding_client));

  if (!resource_request.report_raw_headers) {
    resource_response_->raw_request_response_info = nullptr;
  }

  // We are serving, so if the request is complete before serving, mark the
  // request completion time as now.
  if (status_) {
    status_->completion_time = base::TimeTicks::Now();
  }

  forwarding_client_->OnReceiveResponse(std::move(resource_response_));
  RunEventQueue();
}

void StreamingSearchPrefetchURLLoader::OnReceiveEarlyHints(
    network::mojom::EarlyHintsPtr early_hints) {
  // Do nothing.
}

void StreamingSearchPrefetchURLLoader::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr head) {
  DCHECK(!forwarding_client_);
  DCHECK(streaming_prefetch_request_);

  // Store head and pause new messages until the forwarding client is set up.
  resource_response_ = std::move(head);
  estimated_length_ = resource_response_->content_length < 0
                          ? 0
                          : resource_response_->content_length;
  if (estimated_length_ > 0)
    body_content_.reserve(estimated_length_);

  if (!streaming_prefetch_request_->CanServePrefetchRequest(
          resource_response_->headers)) {
    // Not safe to do anything after this point
    streaming_prefetch_request_->ErrorEncountered();
    return;
  }

  streaming_prefetch_request_->MarkPrefetchAsServable();
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
  if (forwarding_client_) {
    DCHECK(forwarding_client_);
    forwarding_client_->OnTransferSizeUpdated(transfer_size_diff);
    return;
  }
  estimated_length_ += transfer_size_diff;
  if (estimated_length_ > 0)
    body_content_.reserve(estimated_length_);
  event_queue_.push_back(
      base::BindOnce(&StreamingSearchPrefetchURLLoader::OnTransferSizeUpdated,
                     base::Unretained(this), transfer_size_diff));
}

void StreamingSearchPrefetchURLLoader::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  if (forwarding_client_) {
    DCHECK(forwarding_client_);
    DCHECK(!streaming_prefetch_request_);
    forwarding_client_->OnStartLoadingResponseBody(std::move(body));
    return;
  }

  serving_from_data_ = true;

  pipe_drainer_ =
      std::make_unique<mojo::DataPipeDrainer>(this, std::move(body));

  event_queue_.push_back(base::BindOnce(
      &StreamingSearchPrefetchURLLoader::OnStartLoadingResponseBodyFromData,
      base::Unretained(this)));
}

void StreamingSearchPrefetchURLLoader::OnDataAvailable(const void* data,
                                                       size_t num_bytes) {
  body_content_.append(std::string(static_cast<const char*>(data), num_bytes));
  bytes_of_raw_data_to_transfer_ += num_bytes;

  if (forwarding_client_)
    PushData();
}

void StreamingSearchPrefetchURLLoader::OnDataComplete() {
  drain_complete_ = true;

  // Disconnect if all content is served.
  if (bytes_of_raw_data_to_transfer_ - write_position_ == 0) {
    Finish();
  }
}

void StreamingSearchPrefetchURLLoader::OnStartLoadingResponseBodyFromData() {
  mojo::ScopedDataPipeConsumerHandle consumer_handle;

  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = network::kDataPipeDefaultAllocationSize;

  MojoResult rv =
      mojo::CreateDataPipe(&options, producer_handle_, consumer_handle);

  if (rv != MOJO_RESULT_OK) {
    delete this;
    return;
  }

  handle_watcher_ = std::make_unique<mojo::SimpleWatcher>(
      FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL,
      base::SequencedTaskRunnerHandle::Get());
  handle_watcher_->Watch(
      producer_handle_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
      MOJO_WATCH_CONDITION_SATISFIED,
      base::BindRepeating(&StreamingSearchPrefetchURLLoader::OnHandleReady,
                          weak_factory_.GetWeakPtr()));

  forwarding_client_->OnStartLoadingResponseBody(std::move(consumer_handle));

  PushData();
}

void StreamingSearchPrefetchURLLoader::OnHandleReady(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  if (result != MOJO_RESULT_OK) {
    delete this;
    return;
  }
  PushData();
}

void StreamingSearchPrefetchURLLoader::PushData() {
  while (true) {
    DCHECK_GE(bytes_of_raw_data_to_transfer_, write_position_);
    uint32_t write_size =
        static_cast<uint32_t>(bytes_of_raw_data_to_transfer_ - write_position_);
    if (write_size == 0) {
      if (drain_complete_)
        Finish();
      return;
    }

    MojoResult result =
        producer_handle_->WriteData(body_content_.data() + write_position_,
                                    &write_size, MOJO_WRITE_DATA_FLAG_NONE);
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      handle_watcher_->ArmOrNotify();
      return;
    }

    if (result != MOJO_RESULT_OK) {
      delete this;
      return;
    }

    // |write_position_| should only be updated when the mojo pipe has
    // successfully been written to.
    write_position_ += write_size;
  }
}

void StreamingSearchPrefetchURLLoader::Finish() {
  serving_from_data_ = false;
  handle_watcher_.reset();
  producer_handle_.reset();
  if (status_) {
    forwarding_client_->OnComplete(status_.value());
  }
}

void StreamingSearchPrefetchURLLoader::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  network_url_loader_.reset();
  if (forwarding_client_ && !serving_from_data_) {
    DCHECK(!streaming_prefetch_request_);
    forwarding_client_->OnComplete(status);
    return;
  }

  if (streaming_prefetch_request_) {
    DCHECK(!forwarding_client_);
    if (status.error_code == net::OK) {
      streaming_prefetch_request_->MarkPrefetchAsComplete();
    } else {
      streaming_prefetch_request_->ErrorEncountered();
      return;
    }
  }

  status_ = status;
}

void StreamingSearchPrefetchURLLoader::RunEventQueue() {
  for (auto& event : event_queue_) {
    std::move(event).Run();
  }
  event_queue_.clear();
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
  if (network_url_loader_)
    network_url_loader_->SetPriority(priority, intra_priority_value);
}

void StreamingSearchPrefetchURLLoader::PauseReadingBodyFromNet() {
  // Pass through.
  if (network_url_loader_)
    network_url_loader_->PauseReadingBodyFromNet();
}

void StreamingSearchPrefetchURLLoader::ResumeReadingBodyFromNet() {
  // Pass through.
  if (network_url_loader_)
    network_url_loader_->ResumeReadingBodyFromNet();
}

void StreamingSearchPrefetchURLLoader::OnURLLoaderMojoDisconnect() {
  if (!network_url_loader_) {
    // The connection should close after complete.
    return;
  }

  if (!forwarding_client_) {
    DCHECK(streaming_prefetch_request_);
    streaming_prefetch_request_->ErrorEncountered();
  } else {
    delete this;
  }
}

void StreamingSearchPrefetchURLLoader::OnURLLoaderClientMojoDisconnect() {
  DCHECK(forwarding_client_);
  DCHECK(!streaming_prefetch_request_);
  delete this;
}

void StreamingSearchPrefetchURLLoader::ClearOwnerPointer() {
  streaming_prefetch_request_ = nullptr;
}
