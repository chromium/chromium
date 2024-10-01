// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/search_prefetch/streaming_search_prefetch_url_loader.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/field_trial_settings.h"
#include "chrome/browser/preloading/prerender/prerender_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/buildflags/buildflags.h"
#include "mojo/public/c/system/data_pipe.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/record_ontransfersizeupdate_utils.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/url_loader_factory_builder.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/api/web_request/web_request_api.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace {

bool CanServePrefetchRequest(const net::HttpResponseHeaders* const headers,
                             const mojo::ScopedDataPipeConsumerHandle& body) {
  if (!headers || !body) {
    return false;
  }

  // Any 200 response can be served.
  return headers->response_code() >= net::HTTP_OK &&
         headers->response_code() < net::HTTP_MULTIPLE_CHOICES;
}

MojoResult CreateDataPipeForServingData(
    mojo::ScopedDataPipeProducerHandle& producer_handle,
    mojo::ScopedDataPipeConsumerHandle& consumer_handle) {
  MojoCreateDataPipeOptions options;

  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes =
      network::features::GetDataPipeDefaultAllocationSize();

  return mojo::CreateDataPipe(&options, producer_handle, consumer_handle);
}

}  // namespace

StreamingSearchPrefetchURLLoader::ResponseReader::ResponseReader(
    mojo::PendingReceiver<network::mojom::URLLoader> forward_receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> forwarding_client,
    base::OnceCallback<void(ResponseReader*)> forwarding_disconnection_callback,
    std::optional<network::URLLoaderCompletionStatus> status,
    scoped_refptr<StreamingSearchPrefetchURLLoader> loader)
    : disconnection_callback_(std::move(forwarding_disconnection_callback)),
      loader_(std::move(loader)),
      url_loader_completion_status_(status) {
  forwarding_receiver_.Bind(std::move(forward_receiver));
  forwarding_client_.Bind(std::move(forwarding_client));
  // Safe to use Unretained, because `this` owns the receiver.
  forwarding_receiver_.set_disconnect_handler(
      base::BindOnce(&StreamingSearchPrefetchURLLoader::ResponseReader::
                         OnForwardingDisconnection,
                     base::Unretained(this)));
  if (url_loader_completion_status_) {
    // The prefetch request is completed before serving to the prerender
    // navigation, so mark the completion time as now.
    url_loader_completion_status_->completion_time = base::TimeTicks::Now();
  }
}

StreamingSearchPrefetchURLLoader::ResponseReader::~ResponseReader() {
  // Always ensure we recorded something on destruction.
  OnDestroyed();
  // It should be rare, but an edge case can be `loader_` is going to create
  // another `ResponseReader`, in th is case `loader_` should not be deleted and
  // it should be safe. But we'd better delete the reference asynchronously for
  // safety consideration.
  ReleaseSelfReference();

  // TODO(crbug.com/40250486): For now prerender is the only use case. After
  // refactoring it should specify the client type.
  base::UmaHistogramEnumeration(
      "Omnibox.SearchPreload.ResponseDataReaderFinalStatus.Prerender", status_);
}

void StreamingSearchPrefetchURLLoader::ResponseReader::OnStatusCodeReady(
    const network::URLLoaderCompletionStatus& status) {
  DCHECK(!url_loader_completion_status_);
  url_loader_completion_status_ = status;
  if (url_loader_completion_status_->error_code != net::OK) {
    status_ = ResponseDataReaderStatus::kNetworkError;
  }
  MaybeSendCompletionSignal();
}

void StreamingSearchPrefetchURLLoader::ResponseReader::
    StartReadingResponseFromData(
        network::mojom::URLResponseHeadPtr& resource_response) {
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  MojoResult rv =
      CreateDataPipeForServingData(producer_handle_, consumer_handle);
  if (rv != MOJO_RESULT_OK) {
    status_ = ResponseDataReaderStatus::kServingError;
    OnForwardingDisconnection();
    return;
  }
  handle_watcher_ = std::make_unique<mojo::SimpleWatcher>(
      FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL,
      base::SequencedTaskRunner::GetCurrentDefault());
  // It is safe to use `base::Unretained(this)` as `this` owns the watcher.
  handle_watcher_->Watch(
      producer_handle_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
      MOJO_WATCH_CONDITION_SATISFIED,
      base::BindRepeating(
          &StreamingSearchPrefetchURLLoader::ResponseReader::OnDataHandleReady,
          base::Unretained(this)));
  CHECK(resource_response);
  forwarding_client_->OnReceiveResponse(resource_response->Clone(),
                                        std::move(consumer_handle),
                                        /*cached_metadata=*/std::nullopt);
}

void StreamingSearchPrefetchURLLoader::ResponseReader::PushData() {
  if (!loader_) {
    // This will be deleted soon.
    return;
  }
  while (true) {
    std::string_view response_data =
        loader_->GetMoreDataFromCache(write_position_);
    if (response_data.empty()) {
      if (!response_data.data()) {
        complete_writing_ = true;
        MaybeSendCompletionSignal();
      }
      break;
    }
    size_t actually_written_bytes = 0;
    MojoResult result = producer_handle_->WriteData(
        base::as_byte_span(response_data), MOJO_WRITE_DATA_FLAG_NONE,
        actually_written_bytes);

    if (result == MOJO_RESULT_SHOULD_WAIT) {
      handle_watcher_->ArmOrNotify();
      return;
    }

    if (result != MOJO_RESULT_OK) {
      OnForwardingDisconnection();
      // This case is usually caused by the client stopping loading.
      status_ = ResponseDataReaderStatus::kServingError;
      return;
    }

    // |write_position_| should only be updated when the Mojo pipe has
    // successfully been written to.
    write_position_ += actually_written_bytes;
  }
}

void StreamingSearchPrefetchURLLoader::ResponseReader::OnDataHandleReady(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  if (complete_writing_) {
    // This method might be called after this finishes serving with an error
    // result, in which case we do not want to do anything.
    return;
  }
  if (result != MOJO_RESULT_OK) {
    status_ = ResponseDataReaderStatus::kServingError;
    OnForwardingDisconnection();
    return;
  }
  PushData();
}

void StreamingSearchPrefetchURLLoader::ResponseReader::
    MaybeSendCompletionSignal() {
  if (!complete_writing_ || !url_loader_completion_status_) {
    return;
  }
  if (producer_handle_) {
    if (url_loader_completion_status_->error_code == net::OK) {
      status_ = ResponseDataReaderStatus::kCompleted;
    }
    forwarding_client_->OnComplete(*url_loader_completion_status_);
  }
  producer_handle_.reset();
}

void StreamingSearchPrefetchURLLoader::ResponseReader::
    OnForwardingDisconnection() {
  if (!disconnection_callback_) {
    return;
  }
  // If we receive the disconnection signal before completing serving, there
  // should be a serving error.
  if (status_ == ResponseDataReaderStatus::kCreated) {
    status_ = ResponseDataReaderStatus::kServingError;
  }
  // Clear the reference before asking `loader` to delete `this`.
  // See the comment on `loader_`.
  ReleaseSelfReference();
  std::move(disconnection_callback_).Run(this);
}

void StreamingSearchPrefetchURLLoader::ResponseReader::OnDestroyed() {
  switch (status_) {
    // Has completed serving.
    case ResponseDataReaderStatus::kCompleted:
    // For tracking failures.
    case ResponseDataReaderStatus::kServingError:
    case ResponseDataReaderStatus::kNetworkError:
    case ResponseDataReaderStatus::kCanceledByLoader:
      return;
    // The `StreamingSearchPrefetchURLLoader` is destroyed, or it wants to
    // create a new reader and serves to a new clients, so this instance is
    // destroyed.
    case ResponseDataReaderStatus::kCreated:
      status_ = ResponseDataReaderStatus::kCanceledByLoader;
      return;
  }
}

void StreamingSearchPrefetchURLLoader::ResponseReader::ReleaseSelfReference() {
  base::SequencedTaskRunner::GetCurrentDefault()->ReleaseSoon(
      FROM_HERE, std::move(loader_));
}

void StreamingSearchPrefetchURLLoader::ResponseReader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const std::optional<GURL>& new_url) {}
void StreamingSearchPrefetchURLLoader::ResponseReader::SetPriority(
    net::RequestPriority priority,
    int32_t intra_priority_value) {}
// TODO(crbug.com/40250486): We may need to pause the producer from
// pushing data to the client.
void StreamingSearchPrefetchURLLoader::ResponseReader::
    PauseReadingBodyFromNet() {}
void StreamingSearchPrefetchURLLoader::ResponseReader::
    ResumeReadingBodyFromNet() {}

StreamingSearchPrefetchURLLoader::StreamingSearchPrefetchURLLoader(
    SearchPrefetchRequest* streaming_prefetch_request,
    Profile* profile,
    bool navigation_prefetch,
    std::unique_ptr<network::ResourceRequest> resource_request,
    const net::NetworkTrafficAnnotationTag& network_traffic_annotation,
    base::OnceCallback<void(bool)> report_error_callback)
    : streaming_prefetch_request_(streaming_prefetch_request),
      report_error_callback_(std::move(report_error_callback)),
      network_traffic_annotation_(network_traffic_annotation),
      navigation_prefetch_(navigation_prefetch) {
  DCHECK(streaming_prefetch_request_);
  if (navigation_prefetch_ || SearchPrefetchBlockBeforeHeadersIsEnabled()) {
    if (!navigation_prefetch_ &&
        SearchPrefetchBlockHeadStart() > base::TimeDelta()) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
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

  // Maybe proxies the prefetch URL loader via the Extension Web Request API, so
  // that extensions can be informed of any prefetches.
  network::URLLoaderFactoryBuilder factory_builder;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  auto* web_request_api =
      extensions::BrowserContextKeyedAPIFactory<extensions::WebRequestAPI>::Get(
          profile);
  if (web_request_api) {
    web_request_api->MaybeProxyURLLoaderFactory(
        profile, /*frame=*/nullptr, /*render_process_id=*/0,
        content::ContentBrowserClient::URLLoaderFactoryType::kPrefetch,
        /*navigation_id=*/std::nullopt, ukm::kInvalidSourceIdObj,
        factory_builder, /*header_client=*/nullptr,
        /*navigation_response_task_runner=*/nullptr,
        /*request_initiator=*/url::Origin());
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  url_loader_factory_ =
      std::move(factory_builder)
          .Finish(profile->GetDefaultStoragePartition()
                      ->GetURLLoaderFactoryForBrowserProcess());

  // Create a network service URL loader with passed in params.
  url_loader_factory_->CreateLoaderAndStart(
      network_url_loader_.BindNewPipeAndPassReceiver(), 0,
      network::mojom::kURLLoadOptionSendSSLInfoWithResponse |
          network::mojom::kURLLoadOptionSniffMimeType |
          network::mojom::kURLLoadOptionSendSSLInfoForCertificateError,
      *resource_request,
      url_loader_receiver_.BindNewPipeAndPassRemote(
          base::SingleThreadTaskRunner::GetCurrentDefault()),
      net::MutableNetworkTrafficAnnotationTag(network_traffic_annotation_));
  url_loader_receiver_.set_disconnect_handler(base::BindOnce(
      &StreamingSearchPrefetchURLLoader::OnURLLoaderMojoDisconnect,
      base::Unretained(this)));
}

StreamingSearchPrefetchURLLoader::~StreamingSearchPrefetchURLLoader() {
  network_url_loader_.reset();
  url_loader_receiver_.reset();

  // Record the times of serving to prerendering navigation. It should be a
  // small number (0 or 1 in most cases), so cap it at 9.
  int bucket_size = 10;
  base::UmaHistogramCustomCounts(
      "Prerender.Experimental.Search.ResponseReuseCount",
      std::min(count_prerender_serving_times_, bucket_size - 1), /* min= */ 0,
      /* exclusive_max= */ bucket_size, bucket_size);
  if (on_destruction_callback_for_testing_) {
    std::move(on_destruction_callback_for_testing_).Run();
  }
}

// static
SearchPrefetchURLLoader::RequestHandler
StreamingSearchPrefetchURLLoader::GetCallbackForReadingViaResponseReader(
    scoped_refptr<StreamingSearchPrefetchURLLoader> loader) {
  return base::BindOnce(
      &StreamingSearchPrefetchURLLoader::CreateResponseReaderForPrerender,
      std::move(loader));
}

// static
SearchPrefetchURLLoader::RequestHandler
StreamingSearchPrefetchURLLoader::GetServingResponseHandler(
    scoped_refptr<StreamingSearchPrefetchURLLoader> loader) {
  DCHECK(!loader->streaming_prefetch_request_);
  DCHECK(!loader->forwarding_client_);
  loader->RecordInterceptionTime();
  return base::BindOnce(
      &StreamingSearchPrefetchURLLoader::SetUpForwardingClient,
      std::move(loader));
}

void StreamingSearchPrefetchURLLoader::MarkPrefetchAsServable() {
  if (marked_as_servable_) {
    return;
  }
  DCHECK(streaming_prefetch_request_);
  marked_as_servable_ = true;
  streaming_prefetch_request_->MarkPrefetchAsServable();
}

void StreamingSearchPrefetchURLLoader::OnServableResponseCodeReceived() {
  // This means that the navigation stack is already running for the navigation
  // to this term, and chrome does not need to prerender.
  if (!streaming_prefetch_request_) {
    return;
  }
  streaming_prefetch_request_->OnServableResponseCodeReceived();
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
    const network::ResourceRequest& resource_request,
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> forwarding_client) {
  CHECK(!streaming_prefetch_request_);
  // Bind to the content/ navigation code.
  CHECK(!receiver_.is_bound());

  CHECK(!is_activated_);
  is_activated_ = true;

  if (network_url_loader_) {
    network_url_loader_->SetPriority(resource_request.priority, -1);
  }

  // Copy the navigation request for fallback.
  resource_request_ =
      std::make_unique<network::ResourceRequest>(resource_request);

  RecordNavigationURLHistogram(resource_request_->url);

  // Let `this` own itself, so that it can manage its lifetime properly.
  self_pointer_ = WrapRefCounted(this);
  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(base::BindOnce(
      &StreamingSearchPrefetchURLLoader::OnURLLoaderClientMojoDisconnect,
      weak_factory_.GetWeakPtr()));
  forwarding_client_.Bind(std::move(forwarding_client));
  forwarding_result_ = ForwardingResult::kStartedServing;

  // If the object previously encountered an error while still owned elsewhere,
  // schedule the delete for now.
  if (pending_delete_) {
    PostTaskToReleaseOwnership();
    return;
  }

  // In the edge case we were between owners when a response error happened,
  // fallback was deferred until here.
  if (is_scheduled_to_fallback_) {
    Fallback();
  }

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

void StreamingSearchPrefetchURLLoader::CreateResponseReaderForPrerender(
    const network::ResourceRequest& resource_request,
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> forwarding_client) {
  count_prerender_serving_times_++;
  response_reader_for_prerender_ = std::make_unique<ResponseReader>(
      std::move(receiver), std::move(forwarding_client),
      base::BindOnce(
          &StreamingSearchPrefetchURLLoader::OnPrerenderForwardingDisconnect,
          weak_factory_.GetWeakPtr()),
      status_, base::WrapRefCounted(this));
  response_reader_for_prerender_->StartReadingResponseFromData(
      resource_response_);
  response_reader_for_prerender_->PushData();
}

void StreamingSearchPrefetchURLLoader::OnPrerenderForwardingDisconnect(
    ResponseReader* reader) {
  if (reader != response_reader_for_prerender_.get()) {
    return;
  }
  base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(
      FROM_HERE, std::move(response_reader_for_prerender_));
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

void StreamingSearchPrefetchURLLoader::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr head,
    mojo::ScopedDataPipeConsumerHandle body,
    std::optional<mojo_base::BigBuffer> cached_metadata) {
  bool can_be_served = CanServePrefetchRequest(head->headers.get(), body);

  if (is_activated_) {
    std::string histogram_name =
        "Omnibox.SearchPrefetch.ReceivedServableResponse2.";
    histogram_name.append(is_in_fallback_ ? "Fallback." : "Initial.");
    histogram_name.append(
        (navigation_prefetch_ ? "NavigationPrefetch" : "SuggestionPrefetch"));
    base::UmaHistogramBoolean(histogram_name, can_be_served);
  }

  // Cached metadata is not supported for navigation loader.
  cached_metadata.reset();

  // Once we are using the fallback path, just forward calls.
  if (is_in_fallback_) {
    DCHECK(!streaming_prefetch_request_);
    DCHECK(forwarding_client_);
    forwarding_client_->OnReceiveResponse(std::move(head), std::move(body),
                                          std::nullopt);
    return;
  }

  // Don't report errors for navigation prefetch.
  if (!navigation_prefetch_) {
    std::move(report_error_callback_).Run(!can_be_served);
  }

  // If there is an error, either cancel the request or fallback depending on
  // whether we still have a parent pointer.
  if (!can_be_served) {
    if (!streaming_prefetch_request_) {
      // That `streaming_prefetch_request_` is nullptr means this loader is
      // serving to network stack. And we can serve a loader that has not
      // received response yet to the network stack iff the loader was created
      // by a navigation prefetch or we are experimenting with
      // kSearchPrefetchBlockBeforeHeaders enabled.
      DCHECK(navigation_prefetch_ ||
             SearchPrefetchBlockBeforeHeadersIsEnabled());

      // SetUpForwardingClient() needs to be called before fallback.
      if (is_activated_) {
        Fallback();
      } else {
        // Wait until SetUpForwardingClient() is called.
        CHECK(!is_scheduled_to_fallback_);
        is_scheduled_to_fallback_ = true;
      }
      return;
    }
    DCHECK(streaming_prefetch_request_);
    streaming_prefetch_request_->ErrorEncountered();
    return;
    // Not safe to do anything after this point
  }

  if (forwarding_client_) {
    forwarding_client_->OnReceiveResponse(std::move(head), std::move(body),
                                          std::nullopt);
    return;
  }

  MarkPrefetchAsServable();

  // Store head and pause new messages until the forwarding client is set up.
  resource_response_ = std::move(head);

  // Start prerender since here.
  OnServableResponseCodeReceived();

  estimated_length_ = resource_response_->content_length < 0
                          ? 0
                          : resource_response_->content_length;
  if (estimated_length_ > 0) {
    body_content_.reserve(estimated_length_);
  }

  serving_from_data_ = true;

  pipe_drainer_ =
      std::make_unique<mojo::DataPipeDrainer>(this, std::move(body));

  event_queue_.push_back(base::BindOnce(
      &StreamingSearchPrefetchURLLoader::OnStartLoadingResponseBodyFromData,
      base::Unretained(this)));
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
    // Do nothing after this point, as `this` might be deleted.
  } else {
    forwarding_result_ = ForwardingResult::kFailed;
    PostTaskToReleaseOwnership();
  }
}

void StreamingSearchPrefetchURLLoader::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback callback) {
  // We only handle GETs.
  NOTREACHED_IN_MIGRATION();
}

void StreamingSearchPrefetchURLLoader::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {
  network::RecordOnTransferSizeUpdatedUMA(
      network::OnTransferSizeUpdatedFrom::kStreamingSearchPrefetchURLLoader);
  if (forwarding_client_) {
    DCHECK(forwarding_client_);
    forwarding_client_->OnTransferSizeUpdated(transfer_size_diff);
    return;
  }
  estimated_length_ += transfer_size_diff;
  if (estimated_length_ > 0) {
    body_content_.reserve(estimated_length_);
  }
  event_queue_.push_back(
      base::BindOnce(&StreamingSearchPrefetchURLLoader::OnTransferSizeUpdated,
                     base::Unretained(this), transfer_size_diff));
}

void StreamingSearchPrefetchURLLoader::OnDataAvailable(
    base::span<const uint8_t> data) {
  body_content_.append(base::as_string_view(data));

  if (forwarding_client_) {
    PushData();
  }

  if (response_reader_for_prerender_) {
    response_reader_for_prerender_->PushData();
  }
}

void StreamingSearchPrefetchURLLoader::OnDataComplete() {
  drain_complete_ = true;

  // Disconnect if all content is served.
  if (write_position_ == body_content_.size() && forwarding_client_) {
    Finish();
  }
  if (response_reader_for_prerender_) {
    response_reader_for_prerender_->PushData();
  }
}

void StreamingSearchPrefetchURLLoader::OnStartLoadingResponseBodyFromData() {
  DCHECK(forwarding_client_);
  DCHECK(!streaming_prefetch_request_);
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  MojoResult rv =
      CreateDataPipeForServingData(producer_handle_, consumer_handle);

  if (rv != MOJO_RESULT_OK) {
    forwarding_result_ = ForwardingResult::kFailed;
    PostTaskToReleaseOwnership();
    return;
  }

  handle_watcher_ = std::make_unique<mojo::SimpleWatcher>(
      FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL,
      base::SequencedTaskRunner::GetCurrentDefault());
  handle_watcher_->Watch(
      producer_handle_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
      MOJO_WATCH_CONDITION_SATISFIED,
      base::BindRepeating(&StreamingSearchPrefetchURLLoader::OnHandleReady,
                          weak_factory_.GetWeakPtr()));
  CHECK(resource_response_);
  forwarding_client_->OnReceiveResponse(
      resource_response_->Clone(), std::move(consumer_handle), std::nullopt);

  PushData();
}

void StreamingSearchPrefetchURLLoader::OnHandleReady(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  DCHECK(!streaming_prefetch_request_);
  // In the early shutdown pathway, |this| will be deleted soon.
  if (!forwarding_client_) {
    return;
  }
  if (result == MOJO_RESULT_OK) {
    PushData();
    return;
  }
  // This loader has pushed all bytes to the clients, do nothing in this case.
  if (!serving_from_data_) {
    return;
  }
  forwarding_result_ = ForwardingResult::kFailed;
  PostTaskToReleaseOwnership();
}

std::string_view StreamingSearchPrefetchURLLoader::GetMoreDataFromCache(
    size_t writing_position) const {
  if (drain_complete_ && writing_position == body_content_.size()) {
    return std::string_view();
  }
  return std::string_view(body_content_).substr(writing_position);
}

void StreamingSearchPrefetchURLLoader::PushData() {
  // TODO(crbug.com/40250486): This method should be migrated into
  // `ResponseReader::PushData`. Now `ResponseReader` is sort of a copy of this,
  // as we are at the intermediate state during refactoring.
  DCHECK(forwarding_client_);
  DCHECK(!streaming_prefetch_request_);
  while (true) {
    std::string_view response_data = GetMoreDataFromCache(write_position_);

    if (response_data.empty()) {
      // If no data is provided, the cache has served every byte to loader.
      // In this case we can stop.
      if (!response_data.data()) {
        Finish();
      }
      // No data can be fed into the producer.
      return;
    }
    size_t actually_written_bytes = 0;
    MojoResult result = producer_handle_->WriteData(
        base::as_byte_span(response_data), MOJO_WRITE_DATA_FLAG_NONE,
        actually_written_bytes);

    if (result == MOJO_RESULT_SHOULD_WAIT) {
      handle_watcher_->ArmOrNotify();
      return;
    }

    if (result != MOJO_RESULT_OK) {
      forwarding_result_ = ForwardingResult::kFailed;
      PostTaskToReleaseOwnership();
      return;
    }

    // |write_position_| should only be updated when the mojo pipe has
    // successfully been written to.
    write_position_ += actually_written_bytes;
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
    forwarding_result_ = ForwardingResult::kCompleted;
    OnForwardingComplete();
  }
}

void StreamingSearchPrefetchURLLoader::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  TRACE_EVENT0("loading", "StreamingSearchPrefetchURLLoader::OnComplete");
  network_url_loader_.reset();
  status_ = status;
  if (response_reader_for_prerender_) {
    response_reader_for_prerender_->OnStatusCodeReady(status);
  }
  if (forwarding_client_ && (!serving_from_data_ || is_in_fallback_)) {
    DCHECK(!streaming_prefetch_request_);
    forwarding_client_->OnComplete(status);
    forwarding_result_ = ForwardingResult::kCompleted;
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
      // Not safe to do anything after this point.
      return;
    }
  }
}

void StreamingSearchPrefetchURLLoader::RunEventQueue() {
  CHECK(forwarding_client_);
  CHECK(!streaming_prefetch_request_);
  for (auto& event : event_queue_) {
    std::move(event).Run();
    if (!forwarding_client_) {
      // The null forwarding client indicates that the event failed for some
      // reason. Stop processing the remaining events.
      break;
    }
  }
  event_queue_.clear();
}

void StreamingSearchPrefetchURLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const std::optional<GURL>& new_url) {
  if (is_in_fallback_) {
    DCHECK(network_url_loader_);
    network_url_loader_->FollowRedirect(removed_headers, modified_headers,
                                        modified_cors_exempt_headers, new_url);
    return;
  }
  // This should never be called for a non-network service URLLoader.
  NOTREACHED_IN_MIGRATION();
}

void StreamingSearchPrefetchURLLoader::SetPriority(
    net::RequestPriority priority,
    int32_t intra_priority_value) {
  // Pass through.
  if (network_url_loader_) {
    network_url_loader_->SetPriority(priority, intra_priority_value);
  }
}

void StreamingSearchPrefetchURLLoader::PauseReadingBodyFromNet() {
  paused_ = true;
  // Pass through.
  if (network_url_loader_) {
    network_url_loader_->PauseReadingBodyFromNet();
  }
}

void StreamingSearchPrefetchURLLoader::ResumeReadingBodyFromNet() {
  paused_ = false;
  // Pass through.
  if (network_url_loader_) {
    network_url_loader_->ResumeReadingBodyFromNet();
  }
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
    PostTaskToReleaseOwnership();
  }
}

void StreamingSearchPrefetchURLLoader::OnURLLoaderClientMojoDisconnect() {
  DCHECK(forwarding_client_);
  DCHECK(!streaming_prefetch_request_);
  forwarding_client_.reset();
  receiver_.reset();
  // The forwarding logic has finished, so `this` does not need to keep the
  // reference. For prerendering serving, `ResponseReader::loader_` keeps
  // another reference pointer.
  PostTaskToReleaseOwnership();
}

void StreamingSearchPrefetchURLLoader::ClearOwnerPointer() {
  streaming_prefetch_request_ = nullptr;
}

void StreamingSearchPrefetchURLLoader::PostTaskToReleaseOwnership() {
  forwarding_client_.reset();
  receiver_.reset();

  if (!self_pointer_) {
    pending_delete_ = true;
    return;
  }

  if (forwarding_result_ != ForwardingResult::kNotServed) {
    base::UmaHistogramEnumeration(
        count_prerender_serving_times_
            ? "Omnibox.SearchPreload.ForwardingResult.WasServedToPrerender"
            : "Omnibox.SearchPreload.ForwardingResult.NotServedToPrerender",
        forwarding_result_);
  }

  // To avoid UAF bugs, post a separate task to delete this object.
  base::SequencedTaskRunner::GetCurrentDefault()->ReleaseSoon(
      FROM_HERE, std::move(self_pointer_));
}

void StreamingSearchPrefetchURLLoader::Fallback() {
  CHECK(navigation_prefetch_ || SearchPrefetchBlockBeforeHeadersIsEnabled());

  is_scheduled_to_fallback_ = false;

  CHECK(!is_in_fallback_);
  is_in_fallback_ = true;

  // SetUpForwardingClient() should be called before fallback.
  CHECK(is_activated_);
  CHECK(resource_request_);
  CHECK(forwarding_client_);

  network_url_loader_.reset();
  url_loader_receiver_.reset();

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
      &StreamingSearchPrefetchURLLoader::OnURLLoaderMojoDisconnectInFallback,
      base::Unretained(this)));
  if (paused_) {
    network_url_loader_->PauseReadingBodyFromNet();
  }
}

void StreamingSearchPrefetchURLLoader::OnURLLoaderMojoDisconnectInFallback() {
  if (!network_url_loader_) {
    // The connection should close after complete, but we can still be
    // forwarding bytes.
    return;
  }
  PostTaskToReleaseOwnership();
}
