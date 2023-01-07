// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_proxying_url_loader_factory.h"

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_params.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_proxy_configurator.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_service.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_service_factory.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_constants.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/client_hints.h"
#include "services/network/public/cpp/record_ontransfersizeupdate_utils.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "third_party/blink/public/common/client_hints/client_hints.h"

namespace {

const char kAllowedUAClientHint[] = "sec-ch-ua";
const char kAllowedUAMobileClientHint[] = "sec-ch-ua-mobile";

// Little helper class for
// |CheckRedirectsBeforeRunningResourceSuccessfulCallback| since size_t can't be
// ref counted.
class SuccessCount : public base::RefCounted<SuccessCount> {
 public:
  SuccessCount() = default;

  SuccessCount(const SuccessCount&) = delete;
  SuccessCount& operator=(const SuccessCount&) = delete;

  void Increment() { count_++; }
  size_t count() const { return count_; }

 private:
  friend class RefCounted<SuccessCount>;
  ~SuccessCount() = default;

  size_t count_ = 0;
};

// This is the eligibility callback for
// |CheckRedirectsBeforeRunningResourceSuccessfulCallback|. If |eligible| is
// true, then |success_count| is incremented. If |success_count| ever matches
// the size of |resources|, then |callback| is run for every url in |resources|.
void SingleURLEligibilityCheckResult(
    const std::vector<GURL>& resources,
    PrefetchProxyProxyingURLLoaderFactory::ResourceLoadSuccessfulCallback
        callback,
    scoped_refptr<SuccessCount> success_count,
    const GURL& url,
    bool eligible,
    absl::optional<PrefetchProxyPrefetchStatus> not_used) {
  if (eligible) {
    success_count->Increment();
  }

  // If even one url is not eligible, then this if block will never be executed.
  // Once no more callbacks reference the given arguments, they will all be
  // cleaned up and |callback| will be destroyed, never having been run,,
  if (success_count->count() == resources.size()) {
    for (const GURL& resource_url : resources) {
      callback.Run(resource_url);
    }
  }
}

// This method checks every url in |resources|, checking if it is eligible to be
// cached by Prefetch Proxy. If every element is eligible, then all urls are run
// on |callback|. If even a single url is not eligible, |callback| is never run.
void CheckRedirectsBeforeRunningResourceSuccessfulCallback(
    Profile* profile,
    const std::vector<GURL>& resources,
    PrefetchProxyProxyingURLLoaderFactory::ResourceLoadSuccessfulCallback
        callback) {
  DCHECK(profile);
  DCHECK(callback);

  scoped_refptr<SuccessCount> success_count =
      base::MakeRefCounted<SuccessCount>();

  for (const GURL& url : resources) {
    PrefetchProxyTabHelper::CheckEligibilityOfURL(
        profile, url,
        PrefetchType(/*use_isolated_network_context=*/true,
                     /*use_prefetch_proxy=*/true,
                     /*can_prefetch_subresources=*/true),
        base::BindOnce(&SingleURLEligibilityCheckResult, resources, callback,
                       success_count));
  }
}

}  // namespace

PrefetchProxyProxyingURLLoaderFactory::InProgressRequest::InProgressRequest(
    Profile* profile,
    PrefetchProxyProxyingURLLoaderFactory* parent_factory,
    network::mojom::URLLoaderFactory* target_factory,
    ResourceLoadSuccessfulCallback on_resource_load_successful,
    mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
    : profile_(profile),
      parent_factory_(parent_factory),
      on_resource_load_successful_(on_resource_load_successful),
      target_client_(std::move(client)),
      loader_receiver_(this, std::move(loader_receiver)) {
  redirect_chain_.push_back(request.url);

  mojo::PendingRemote<network::mojom::URLLoaderClient> proxy_client =
      client_receiver_.BindNewPipeAndPassRemote();

  target_factory->CreateLoaderAndStart(
      target_loader_.BindNewPipeAndPassReceiver(), request_id, options, request,
      std::move(proxy_client), traffic_annotation);

  // Calls |OnBindingsClosed| only after both disconnect handlers have been run.
  base::RepeatingClosure closure = base::BarrierClosure(
      2, base::BindOnce(&InProgressRequest::OnBindingsClosed,
                        base::Unretained(this)));
  loader_receiver_.set_disconnect_handler(closure);
  client_receiver_.set_disconnect_handler(closure);
}

PrefetchProxyProxyingURLLoaderFactory::InProgressRequest::~InProgressRequest() {
  if (destruction_callback_) {
    std::move(destruction_callback_).Run();
  }
}

void PrefetchProxyProxyingURLLoaderFactory::InProgressRequest::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const absl::optional<GURL>& new_url) {
  target_loader_->FollowRedirect(removed_headers, modified_headers,
                                 modified_cors_exempt_headers, new_url);
}

void PrefetchProxyProxyingURLLoaderFactory::InProgressRequest::SetPriority(
    net::RequestPriority priority,
    int32_t intra_priority_value) {
  target_loader_->SetPriority(priority, intra_priority_value);
}

void PrefetchProxyProxyingURLLoaderFactory::InProgressRequest::
    PauseReadingBodyFromNet() {
  target_loader_->PauseReadingBodyFromNet();
}

void PrefetchProxyProxyingURLLoaderFactory::InProgressRequest::
    ResumeReadingBodyFromNet() {
  target_loader_->ResumeReadingBodyFromNet();
}

void PrefetchProxyProxyingURLLoaderFactory::InProgressRequest::
    OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) {
  target_client_->OnReceiveEarlyHints(std::move(early_hints));
}

void PrefetchProxyProxyingURLLoaderFactory::InProgressRequest::
    OnReceiveResponse(network::mojom::URLResponseHeadPtr head,
                      mojo::ScopedDataPipeConsumerHandle body,
                      absl::optional<mojo_base::BigBuffer> cached_metadata) {
  if (head) {
    head_ = head->Clone();
  }
  target_client_->OnReceiveResponse(std::move(head), std::move(body),
                                    std::move(cached_metadata));
}

void PrefetchProxyProxyingURLLoaderFactory::InProgressRequest::
    OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                      network::mojom::URLResponseHeadPtr head) {
  redirect_chain_.push_back(redirect_info.new_url);
  target_client_->OnReceiveRedirect(redirect_info, std::move(head));
}

void PrefetchProxyProxyingURLLoaderFactory::InProgressRequest::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback callback) {
  target_client_->OnUploadProgress(current_position, total_size,
                                   std::move(callback));
}

void PrefetchProxyProxyingURLLoaderFactory::InProgressRequest::
    OnTransferSizeUpdated(int32_t transfer_size_diff) {
  network::RecordOnTransferSizeUpdatedUMA(
      network::OnTransferSizeUpdatedFrom::
          kPrefetchProxyProxyingURLLoaderFactory);
  target_client_->OnTransferSizeUpdated(transfer_size_diff);
}

void PrefetchProxyProxyingURLLoaderFactory::InProgressRequest::
    SetOnCompleteRecordMetricsCallback(
        OnCompleteRecordMetricsCallback callback) {
  on_complete_metrics_callback_ = std::move(callback);
}

void PrefetchProxyProxyingURLLoaderFactory::InProgressRequest::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  if (on_complete_metrics_callback_) {
    std::move(on_complete_metrics_callback_)
        .Run(redirect_chain_[0], head_ ? head_->Clone() : nullptr, status);
  }
  MaybeReportResourceLoadSuccess(status);
  target_client_->OnComplete(status);
}

void PrefetchProxyProxyingURLLoaderFactory::InProgressRequest::
    OnBindingsClosed() {
  // Destroys |this|.
  parent_factory_->RemoveRequest(this);
}

void PrefetchProxyProxyingURLLoaderFactory::InProgressRequest::
    MaybeReportResourceLoadSuccess(
        const network::URLLoaderCompletionStatus& status) {
  if (status.error_code != net::OK) {
    return;
  }

  if (!head_) {
    return;
  }

  if (!head_->headers) {
    return;
  }

  if (head_->headers->response_code() >= net::HTTP_MULTIPLE_CHOICES) {
    return;
  }

  if (head_->headers->response_code() < net::HTTP_OK) {
    return;
  }

  if (!on_resource_load_successful_) {
    return;
  }

  if (!profile_) {
    return;
  }

  DCHECK_GT(redirect_chain_.size(), 0U);

  // Check each url in the redirect chain before reporting success.
  CheckRedirectsBeforeRunningResourceSuccessfulCallback(
      profile_, redirect_chain_, on_resource_load_successful_);
}

PrefetchProxyProxyingURLLoaderFactory::AbortRequest::~AbortRequest() = default;
PrefetchProxyProxyingURLLoaderFactory::AbortRequest::AbortRequest(
    mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client)
    : target_client_(std::move(client)),
      loader_receiver_(this, std::move(loader_receiver)) {
  loader_receiver_.set_disconnect_handler(base::BindOnce(
      &PrefetchProxyProxyingURLLoaderFactory::AbortRequest::OnBindingClosed,
      base::Unretained(this)));

  // PostTask the failure to get it out of this message loop, allowing the mojo
  // pipes to settle in.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &PrefetchProxyProxyingURLLoaderFactory::AbortRequest::Abort,
          weak_factory_.GetWeakPtr()));
}

void PrefetchProxyProxyingURLLoaderFactory::AbortRequest::Abort() {
  target_client_->OnComplete(
      network::URLLoaderCompletionStatus(net::ERR_FAILED));
}

void PrefetchProxyProxyingURLLoaderFactory::AbortRequest::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const absl::optional<GURL>& new_url) {}
void PrefetchProxyProxyingURLLoaderFactory::AbortRequest::SetPriority(
    net::RequestPriority priority,
    int32_t intra_priority_value) {}
void PrefetchProxyProxyingURLLoaderFactory::AbortRequest::
    PauseReadingBodyFromNet() {}
void PrefetchProxyProxyingURLLoaderFactory::AbortRequest::
    ResumeReadingBodyFromNet() {}

void PrefetchProxyProxyingURLLoaderFactory::AbortRequest::OnBindingClosed() {
  delete this;
}

PrefetchProxyProxyingURLLoaderFactory::PrefetchProxyProxyingURLLoaderFactory(
    ResourceMetricsObserver* metrics_observer,
    int frame_tree_node_id,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> loader_receiver,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        network_process_factory,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> isolated_factory,
    DisconnectCallback on_disconnect,
    ResourceLoadSuccessfulCallback on_resource_load_successful)
    : metrics_observer_(metrics_observer),
      frame_tree_node_id_(frame_tree_node_id),
      on_resource_load_successful_(std::move(on_resource_load_successful)),
      on_disconnect_(std::move(on_disconnect)) {
  DCHECK(metrics_observer_);

  network_process_factory_.Bind(std::move(network_process_factory));
  network_process_factory_.set_disconnect_handler(base::BindOnce(
      &PrefetchProxyProxyingURLLoaderFactory::OnNetworkProcessFactoryError,
      base::Unretained(this)));

  isolated_factory_.Bind(std::move(isolated_factory));
  isolated_factory_.set_disconnect_handler(base::BindOnce(
      &PrefetchProxyProxyingURLLoaderFactory::OnIsolatedFactoryError,
      base::Unretained(this)));

  proxy_receivers_.Add(this, std::move(loader_receiver));
  proxy_receivers_.set_disconnect_handler(base::BindRepeating(
      &PrefetchProxyProxyingURLLoaderFactory::OnProxyBindingError,
      base::Unretained(this)));
}

PrefetchProxyProxyingURLLoaderFactory::
    ~PrefetchProxyProxyingURLLoaderFactory() = default;

void PrefetchProxyProxyingURLLoaderFactory::NotifyPageNavigatedToAfterSRP(
    const std::set<GURL>& cached_subresources) {
  previously_cached_subresources_ = cached_subresources;
}

bool PrefetchProxyProxyingURLLoaderFactory::ShouldHandleRequestForPrerender()
    const {
  return !previously_cached_subresources_.has_value();
}

void PrefetchProxyProxyingURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  content::WebContents* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id_);
  if (!web_contents) {
    return;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  // If this request is happening during a prerender then check if it is
  // eligible for caching before putting it on the network.
  if (ShouldHandleRequestForPrerender()) {
    // Do not allow insecure resources to be fetched due to risk of privacy
    // leaks in an HSTS setting.
    if (!request.url.SchemeIs(url::kHttpsScheme)) {
      auto abort_request = std::make_unique<AbortRequest>(
          std::move(loader_receiver), std::move(client));
      // The request will manage its own lifecycle based on the mojo pipes.
      abort_request.release();
      return;
    }

    // Check if this prerender has exceeded its max number of subresources.
    request_count_++;
    if (request_count_ > PrefetchProxyMaxSubresourcesPerPrerender()) {
      metrics_observer_->OnResourceThrottled(request.url);
      auto abort_request = std::make_unique<AbortRequest>(
          std::move(loader_receiver), std::move(client));
      // The request will manage its own lifecycle based on the mojo pipes.
      abort_request.release();
      return;
    }

    // Check that the proxy server is available. If not, fast abort the request.
    PrefetchProxyService* prefetch_proxy_service =
        PrefetchProxyServiceFactory::GetForProfile(profile);
    if (prefetch_proxy_service && !prefetch_proxy_service->proxy_configurator()
                                       ->IsPrefetchProxyAvailable()) {
      metrics_observer_->OnProxyUnavailableForResource(request.url);
      auto abort_request = std::make_unique<AbortRequest>(
          std::move(loader_receiver), std::move(client));
      // The request will manage its own lifecycle based on the mojo pipes.
      abort_request.release();
      return;
    }

    // We must check if the request can be cached and set the appropriate load
    // flag if so.
    PrefetchProxyTabHelper::CheckEligibilityOfURL(
        profile, request.url,
        PrefetchType(/*use_isolated_network_context=*/true,
                     /*use_prefetch_proxy=*/true,
                     /*can_prefetch_subresources=*/true),
        base::BindOnce(
            &PrefetchProxyProxyingURLLoaderFactory::OnEligibilityResult,
            weak_factory_.GetWeakPtr(), profile, std::move(loader_receiver),
            request_id, options, request, std::move(client),
            traffic_annotation));
    return;
  }

  // This request is happening after the user clicked to a prerendered page.
  DCHECK(previously_cached_subresources_.has_value());
  const std::set<GURL>& cached_subresources = *previously_cached_subresources_;
  if (cached_subresources.find(request.url) != cached_subresources.end()) {
    // Load this resource from |isolated_factory_|'s cache.
    auto in_progress_request = std::make_unique<InProgressRequest>(
        profile, this, isolated_factory_.get(), base::NullCallback(),
        std::move(loader_receiver), request_id, options, request,
        std::move(client), traffic_annotation);
    in_progress_request->SetOnCompleteRecordMetricsCallback(
        base::BindOnce(&PrefetchProxyProxyingURLLoaderFactory::
                           RecordSubresourceMetricsAfterClick,
                       base::Unretained(this)));
    requests_.insert(std::move(in_progress_request));
  } else {
    // Resource was not cached during the NSP, so load it normally.
    // No metrics callback here, since there's nothing important to record.
    requests_.insert(std::make_unique<InProgressRequest>(
        profile, this, network_process_factory_.get(), base::NullCallback(),
        std::move(loader_receiver), request_id, options, request,
        std::move(client), traffic_annotation));
  }
}

void PrefetchProxyProxyingURLLoaderFactory::OnEligibilityResult(
    Profile* profile,
    mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    const GURL& url,
    bool eligible,
    absl::optional<PrefetchProxyPrefetchStatus> status) {
  DCHECK_EQ(request.url, url);
  DCHECK(!previously_cached_subresources_.has_value());
  DCHECK(request.cors_exempt_headers.HasHeader(
      content::kCorsExemptPurposeHeaderName));
  DCHECK(request.load_flags & net::LOAD_PREFETCH);
  DCHECK(!request.trusted_params.has_value());

  network::ResourceRequest isolated_request = request;

  // Ensures that the U-A string is set to the Isolated Network Context's
  // default.
  isolated_request.headers.RemoveHeader("User-Agent");

  // Ensures that the Accept-Language string is set to the Isolated Network
  // Context's default.
  isolated_request.headers.RemoveHeader("Accept-Language");

  // Strip out all Client Hints.
  for (const auto& elem : network::GetClientHintToNameMap()) {
    const auto& header = elem.second;
    // UA Client Hint and UA Mobile are ok to send.
    if (header == kAllowedUAClientHint ||
        header == kAllowedUAMobileClientHint) {
      continue;
    }
    isolated_request.headers.RemoveHeader(header);
  }

  ResourceLoadSuccessfulCallback resource_load_successful_callback =
      on_resource_load_successful_;

  // If this subresource is eligible for prefetching then it can be cached. If
  // not, it must still be put on the wire to avoid privacy attacks but should
  // not be cached or change any cookies.
  if (!eligible) {
    if (status) {
      metrics_observer_->OnResourceNotEligible(url, *status);
    }

    isolated_request.load_flags |= net::LOAD_DISABLE_CACHE;
    isolated_request.credentials_mode = network::mojom::CredentialsMode::kOmit;

    // Don't report loaded resources that won't go in the cache.
    resource_load_successful_callback = base::NullCallback();
  }

  auto in_progress_request = std::make_unique<InProgressRequest>(
      profile, this, isolated_factory_.get(), resource_load_successful_callback,
      std::move(loader_receiver), request_id, options, isolated_request,
      std::move(client), traffic_annotation);
  in_progress_request->SetOnCompleteRecordMetricsCallback(
      base::BindOnce(&PrefetchProxyProxyingURLLoaderFactory::
                         RecordSubresourceMetricsDuringPrerender,
                     base::Unretained(this)));
  requests_.insert(std::move(in_progress_request));
}

void PrefetchProxyProxyingURLLoaderFactory::
    RecordSubresourceMetricsDuringPrerender(
        const GURL& url,
        network::mojom::URLResponseHeadPtr head,
        const network::URLLoaderCompletionStatus& status) {
  base::UmaHistogramSparse("PrefetchProxy.Prefetch.Subresources.NetError",
                           std::abs(status.error_code));
  if (head && head->headers) {
    base::UmaHistogramSparse("PrefetchProxy.Prefetch.Subresources.RespCode",
                             head->headers->response_code());
  }

  metrics_observer_->OnResourceFetchComplete(url, std::move(head), status);
}

void PrefetchProxyProxyingURLLoaderFactory::RecordSubresourceMetricsAfterClick(
    const GURL& url,
    network::mojom::URLResponseHeadPtr head,
    const network::URLLoaderCompletionStatus& status) {
  UMA_HISTOGRAM_BOOLEAN("PrefetchProxy.AfterClick.Subresources.UsedCache",
                        status.exists_in_cache);
  metrics_observer_->OnResourceUsedFromCache(url);
}

void PrefetchProxyProxyingURLLoaderFactory::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> loader_receiver) {
  proxy_receivers_.Add(this, std::move(loader_receiver));
}

void PrefetchProxyProxyingURLLoaderFactory::OnNetworkProcessFactoryError() {
  // Stop calls to CreateLoaderAndStart() when |network_process_factory_| is
  // invalid.
  network_process_factory_.reset();
  proxy_receivers_.Clear();

  MaybeDestroySelf();
}

void PrefetchProxyProxyingURLLoaderFactory::OnIsolatedFactoryError() {
  // Stop calls to CreateLoaderAndStart() when |isolated_factory_| is
  // invalid.
  isolated_factory_.reset();
  proxy_receivers_.Clear();

  MaybeDestroySelf();
}

void PrefetchProxyProxyingURLLoaderFactory::OnProxyBindingError() {
  if (proxy_receivers_.empty()) {
    network_process_factory_.reset();
  }

  MaybeDestroySelf();
}

void PrefetchProxyProxyingURLLoaderFactory::RemoveRequest(
    InProgressRequest* request) {
  auto it = requests_.find(request);
  DCHECK(it != requests_.end());
  requests_.erase(it);

  MaybeDestroySelf();
}

void PrefetchProxyProxyingURLLoaderFactory::MaybeDestroySelf() {
  // Even if all URLLoaderFactory pipes connected to this object have been
  // closed it has to stay alive until all active requests have completed.
  if (network_process_factory_.is_bound() || isolated_factory_.is_bound() ||
      !requests_.empty()) {
    return;
  }

  // Deletes |this|.
  std::move(on_disconnect_).Run(this);
}
