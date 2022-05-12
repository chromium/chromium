// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/search_prefetch/streaming_search_prefetch_url_loader.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/task_traits.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/prefetch/search_prefetch/field_trial_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/c/system/data_pipe.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/constants.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "streaming_search_prefetch_url_loader.h"
#include "url/gurl.h"

namespace {

bool CanServePrefetchRequest(
    const scoped_refptr<net::HttpResponseHeaders> headers) {
  if (!headers)
    return false;

  // Any 200 response can be served.
  if (headers->response_code() >= net::HTTP_OK &&
      headers->response_code() < net::HTTP_MULTIPLE_CHOICES) {
    return true;
  }

  return false;
}

}  // namespace

StreamingSearchPrefetchURLLoader::StreamingSearchPrefetchURLLoader(
    StreamingSearchPrefetchRequest* streaming_prefetch_request,
    Profile* profile,
    bool navigation_prefetch,
    std::unique_ptr<network::ResourceRequest> resource_request,
    const net::NetworkTrafficAnnotationTag& network_traffic_annotation,
    base::OnceCallback<void(bool)> report_error_callback)
    : streaming_prefetch_request_(streaming_prefetch_request),
      report_error_callback_(std::move(report_error_callback)),
      profile_(profile),
      network_traffic_annotation_(network_traffic_annotation),
      navigation_prefetch_(navigation_prefetch) {
  DCHECK(streaming_prefetch_request_);
  if (navigation_prefetch_ || SearchPrefetchBlockBeforeHeadersIsEnabled()) {
    if (!navigation_prefetch_ &&
        SearchPrefetchBlockHeadStart() > base::TimeDelta()) {
      base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(
              &StreamingSearchPrefetchURLLoader::MarkPrefetchAsServable,
              weak_factory_.GetWeakPtr()),
          SearchPrefetchBlockHeadStart());
    } else {
      MarkPrefetchAsServable();
    }
  }
  prefetch_url_ = resource_request->url;
  auto url_loader_factory = profile->GetDefaultStoragePartition()
                                ->GetURLLoaderFactoryForBrowserProcess();

  // Create a network service URL loader with passed in params.
  url_loader_factory->CreateLoaderAndStart(
      network_url_loader_.BindNewPipeAndPassReceiver(), 0,
      network::mojom::kURLLoadOptionSendSSLInfoWithResponse |
          network::mojom::kURLLoadOptionSniffMimeType |
          network::mojom::kURLLoadOptionSendSSLInfoForCertificateError,
      *resource_request,
      url_loader_receiver_.BindNewPipeAndPassRemote(
          base::ThreadTaskRunnerHandle::Get()),
      net::MutableNetworkTrafficAnnotationTag(network_traffic_annotation_));
  url_loader_receiver_.set_disconnect_handler(base::BindOnce(
      &StreamingSearchPrefetchURLLoader::OnURLLoaderMojoDisconnect,
      base::Unretained(this)));
}

StreamingSearchPrefetchURLLoader::~StreamingSearchPrefetchURLLoader() = default;

void StreamingSearchPrefetchURLLoader::MarkPrefetchAsServable() {
  if (marked_as_servable_)
    return;
  DCHECK(streaming_prefetch_request_);
  marked_as_servable_ = true;
  streaming_prefetch_request_->MarkPrefetchAsServable();
}

SearchPrefetchURLLoader::RequestHandler
StreamingSearchPrefetchURLLoader::ServingResponseHandlerImpl(
    std::unique_ptr<SearchPrefetchURLLoader> loader) {
  DCHECK(!streaming_prefetch_request_);
  DCHECK(!forwarding_client_);
  return base::BindOnce(
      &StreamingSearchPrefetchURLLoader::SetUpForwardingClient,
      weak_factory_.GetWeakPtr(), std::move(loader));
}

void StreamingSearchPrefetchURLLoader::RecordNavigationURLHistogram(
    const GURL& navigation_url) {
  if (navigation_prefetch_) {
    UMA_HISTOGRAM_BOOLEAN(
        "Omnibox.SearchPrefetch.NavigationURLMatches.NavigationPrefetch",
        (prefetch_url_ == navigation_url));
  }
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

  // Copy the navigation request for fallback.
  resource_request_ =
      std::make_unique<network::ResourceRequest>(resource_request);

  RecordNavigationURLHistogram(resource_request_->url);

  // At this point, we are bound to the mojo receiver, so we can release
  // |loader|, which points to |this|.
  receiver_.Bind(std::move(receiver));
  self_pointer_ = std::move(loader);
  receiver_.set_disconnect_handler(base::BindOnce(
      &StreamingSearchPrefetchURLLoader::OnURLLoaderClientMojoDisconnect,
      weak_factory_.GetWeakPtr()));
  forwarding_client_.Bind(std::move(forwarding_client));

  // If the object previously encountered an error while still owned elsewhere,
  // schedule the delete for now.
  if (pending_delete_) {
    PostTaskToDeleteSelf();
    return;
  }

  // In the edge case we were between owners when fallback occurred, we need to
  // resume the receiver.
  if (is_in_fallback_)
    url_loader_receiver_.Resume();

  // Headers have not been received yet, we can forward the response if
  // we receive it without error.
  if (!resource_response_) {
    return;
  }

  // We are serving, so if the request is complete before serving, mark the
  // request completion time as now.
  if (status_) {
    status_->completion_time = base::TimeTicks::Now();
  }

  RunEventQueue();
}

void StreamingSearchPrefetchURLLoader::OnReceiveEarlyHints(
    network::mojom::EarlyHintsPtr early_hints) {
  if (is_in_fallback_) {
    DCHECK(!streaming_prefetch_request_);
    DCHECK(forwarding_client_);
    forwarding_client_->OnReceiveEarlyHints(std::move(early_hints));
  }
  // Do nothing.
}

void StreamingSearchPrefetchURLLoader::SetHeadersReceivedCallback(
    base::OnceClosure headers_received_callback) {
  headers_received_callback_ = std::move(headers_received_callback);
}

bool StreamingSearchPrefetchURLLoader::ReadyToServe() {
  return can_be_served_.has_value() && can_be_served_.value();
}

bool StreamingSearchPrefetchURLLoader::ReceivedError() {
  return can_be_served_.has_value() && !(can_be_served_.value());
}

void StreamingSearchPrefetchURLLoader::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr head,
    mojo::ScopedDataPipeConsumerHandle body) {
  // Once we are using the fallback path, just forward calls.
  if (is_in_fallback_) {
    DCHECK(!streaming_prefetch_request_);
    DCHECK(forwarding_client_);
    forwarding_client_->OnReceiveResponse(std::move(head), std::move(body));
    return;
  }

  can_be_served_ = CanServePrefetchRequest(head->headers);

  // Don't report errors for navigation prefetch.
  if (!navigation_prefetch_)
    std::move(report_error_callback_).Run(!can_be_served_.value());

  if (headers_received_callback_) {
    // Stop future messages, this object just needs to keep the request alive in
    // the network service until it is served elsewhere.
    url_loader_receiver_.Pause();
    std::move(headers_received_callback_).Run();
    return;
  }

  // If there is an error, either cancel the request or fallback depending on
  // whether we still have a parent pointer.
  if (!can_be_served_.value()) {
    if ((navigation_prefetch_ || SearchPrefetchBlockBeforeHeadersIsEnabled()) &&
        !streaming_prefetch_request_) {
      Fallback();
      return;
    }
    DCHECK(streaming_prefetch_request_);
    streaming_prefetch_request_->ErrorEncountered();
    return;
    // Not safe to do anything after this point
  }

  if (forwarding_client_) {
    forwarding_client_->OnReceiveResponse(std::move(head), std::move(body));
    return;
  }

  MarkPrefetchAsServable();

  // Store head and pause new messages until the forwarding client is set up.
  resource_response_ = std::move(head);
  estimated_length_ = resource_response_->content_length < 0
                          ? 0
                          : resource_response_->content_length;
  if (estimated_length_ > 0)
    body_content_.reserve(estimated_length_);

  OnStartLoadingResponseBody(std::move(body));
}

void StreamingSearchPrefetchURLLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr head) {
  if (is_in_fallback_) {
    DCHECK(forwarding_client_);
    forwarding_client_->OnReceiveRedirect(redirect_info, std::move(head));
    return;
  }
  if (streaming_prefetch_request_) {
    streaming_prefetch_request_->ErrorEncountered();
  } else {
    PostTaskToDeleteSelf();
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
  DCHECK(!forwarding_client_);
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
  if (bytes_of_raw_data_to_transfer_ - write_position_ == 0 &&
      forwarding_client_) {
    Finish();
  }
}

void StreamingSearchPrefetchURLLoader::OnStartLoadingResponseBodyFromData() {
  DCHECK(forwarding_client_);
  DCHECK(!streaming_prefetch_request_);
  mojo::ScopedDataPipeConsumerHandle consumer_handle;

  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes =
      network::features::GetDataPipeDefaultAllocationSize();

  MojoResult rv =
      mojo::CreateDataPipe(&options, producer_handle_, consumer_handle);

  if (rv != MOJO_RESULT_OK) {
    PostTaskToDeleteSelf();
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

  forwarding_client_->OnReceiveResponse(std::move(resource_response_),
                                        std::move(consumer_handle));

  PushData();
}

void StreamingSearchPrefetchURLLoader::OnHandleReady(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  DCHECK(forwarding_client_);
  DCHECK(!streaming_prefetch_request_);
  if (result != MOJO_RESULT_OK) {
    PostTaskToDeleteSelf();
    return;
  }
  PushData();
}

void StreamingSearchPrefetchURLLoader::PushData() {
  DCHECK(forwarding_client_);
  DCHECK(!streaming_prefetch_request_);
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
      PostTaskToDeleteSelf();
      return;
    }

    // |write_position_| should only be updated when the mojo pipe has
    // successfully been written to.
    write_position_ += write_size;
  }
}

void StreamingSearchPrefetchURLLoader::Finish() {
  DCHECK(forwarding_client_);
  DCHECK(!streaming_prefetch_request_);

  serving_from_data_ = false;
  handle_watcher_.reset();
  producer_handle_.reset();
  if (status_) {
    forwarding_client_->OnComplete(status_.value());
    OnForwardingComplete();
  }
}

void StreamingSearchPrefetchURLLoader::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  network_url_loader_.reset();
  if (forwarding_client_ && (!serving_from_data_ || is_in_fallback_)) {
    DCHECK(!streaming_prefetch_request_);
    forwarding_client_->OnComplete(status);
    OnForwardingComplete();
    return;
  }

  if (streaming_prefetch_request_) {
    DCHECK(!forwarding_client_);
    DCHECK(!self_pointer_);
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
  DCHECK(forwarding_client_);
  DCHECK(!streaming_prefetch_request_);
  for (auto& event : event_queue_) {
    std::move(event).Run();
  }
  event_queue_.clear();
}

void StreamingSearchPrefetchURLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const absl::optional<GURL>& new_url) {
  if (is_in_fallback_) {
    DCHECK(network_url_loader_);
    network_url_loader_->FollowRedirect(removed_headers, modified_headers,
                                        modified_cors_exempt_headers, new_url);
    return;
  }
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
  paused_ = true;
  // Pass through.
  if (network_url_loader_)
    network_url_loader_->PauseReadingBodyFromNet();
}

void StreamingSearchPrefetchURLLoader::ResumeReadingBodyFromNet() {
  paused_ = false;
  // Pass through.
  if (network_url_loader_)
    network_url_loader_->ResumeReadingBodyFromNet();
}

void StreamingSearchPrefetchURLLoader::OnURLLoaderMojoDisconnect() {
  if (!network_url_loader_) {
    // The connection should close after complete.
    return;
  }

  if (is_in_fallback_) {
    // The connection should close after fallback to a different loader.
    return;
  }

  if (streaming_prefetch_request_) {
    DCHECK(!forwarding_client_);
    streaming_prefetch_request_->ErrorEncountered();
  } else {
    PostTaskToDeleteSelf();
  }
}

void StreamingSearchPrefetchURLLoader::OnURLLoaderClientMojoDisconnect() {
  DCHECK(forwarding_client_);
  DCHECK(!streaming_prefetch_request_);
  PostTaskToDeleteSelf();
}

void StreamingSearchPrefetchURLLoader::ClearOwnerPointer() {
  streaming_prefetch_request_ = nullptr;
}

void StreamingSearchPrefetchURLLoader::PostTaskToDeleteSelf() {
  network_url_loader_.reset();
  url_loader_receiver_.reset();

  forwarding_client_.reset();
  receiver_.reset();

  if (!self_pointer_) {
    pending_delete_ = true;
    return;
  }
  // To avoid UAF bugs, post a separate task to delete this object.
  base::SequencedTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE,
                                                     std::move(self_pointer_));
}

void StreamingSearchPrefetchURLLoader::Fallback() {
  DCHECK(!is_in_fallback_);
  DCHECK(navigation_prefetch_ || SearchPrefetchBlockBeforeHeadersIsEnabled());

  network_url_loader_.reset();
  url_loader_receiver_.reset();
  is_in_fallback_ = true;

  auto url_loader_factory = profile_->GetDefaultStoragePartition()
                                ->GetURLLoaderFactoryForBrowserProcess();

  // Create a network service URL loader with passed in params.
  url_loader_factory->CreateLoaderAndStart(
      network_url_loader_.BindNewPipeAndPassReceiver(), 0,
      network::mojom::kURLLoadOptionSendSSLInfoWithResponse |
          network::mojom::kURLLoadOptionSniffMimeType |
          network::mojom::kURLLoadOptionSendSSLInfoForCertificateError,
      *resource_request_,
      url_loader_receiver_.BindNewPipeAndPassRemote(
          base::ThreadTaskRunnerHandle::Get()),
      net::MutableNetworkTrafficAnnotationTag(network_traffic_annotation_));
  url_loader_receiver_.set_disconnect_handler(base::BindOnce(
      &StreamingSearchPrefetchURLLoader::OnURLLoaderMojoDisconnectInFallback,
      base::Unretained(this)));
  if (paused_) {
    network_url_loader_->PauseReadingBodyFromNet();
  }
  // Pause the url loader until we have a forwarding client, this should be
  // rare, but can happen when the callback to this URL Loader is called at a
  // later point than when it taken from the prefetch service.
  if (!forwarding_client_) {
    url_loader_receiver_.Pause();
  }
}

void StreamingSearchPrefetchURLLoader::OnURLLoaderMojoDisconnectInFallback() {
  if (!network_url_loader_) {
    // The connection should close after complete, but we can still be
    // forwarding bytes.
    return;
  }

  PostTaskToDeleteSelf();
}
