// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_PROXYING_URL_LOADER_FACTORY_H_
#define CHROME_BROWSER_PRELOADING_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_PROXYING_URL_LOADER_FACTORY_H_

#include <memory>
#include <set>
#include <vector>

#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_prefetch_status.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

class Profile;

// This class is an intermediary URLLoaderFactory between the renderer and
// network process, AKA proxy which should not be confused with a proxy server.
//
// This class sends all requests to an isolated network context which will strip
// any private information before being sent on the wire. Those requests are
// also monitored for when resource loads complete successfully and reports
// those to the |PrefetchProxySubresourceManager| which owns |this|.
class PrefetchProxyProxyingURLLoaderFactory
    : public network::mojom::URLLoaderFactory {
 public:
  class ResourceMetricsObserver {
   public:
    // Called when the resource finishes, either in failure or success.
    virtual void OnResourceFetchComplete(
        const GURL& url,
        network::mojom::URLResponseHeadPtr head,
        const network::URLLoaderCompletionStatus& status) = 0;

    // Called when a subresource load exceeds the experimental maximum and the
    // load is aborted before going to the network.
    virtual void OnResourceThrottled(const GURL& url) = 0;

    // Called when a subresource could not be loaded because the proxy is
    // unavailable.
    virtual void OnProxyUnavailableForResource(const GURL& url) = 0;

    // Called when a subresource is not eligible to be prefetched.
    virtual void OnResourceNotEligible(const GURL& url,
                                       PrefetchProxyPrefetchStatus status) = 0;

    // Called when a previously prefetched subresource is loaded from the cache.
    virtual void OnResourceUsedFromCache(const GURL& url) = 0;
  };

  using DisconnectCallback =
      base::OnceCallback<void(PrefetchProxyProxyingURLLoaderFactory*)>;

  using ResourceLoadSuccessfulCallback =
      base::RepeatingCallback<void(const GURL& url)>;

  PrefetchProxyProxyingURLLoaderFactory(
      ResourceMetricsObserver* metrics_observer,
      int frame_tree_node_id,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          network_process_factory,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> isolated_factory,
      DisconnectCallback on_disconnect,
      ResourceLoadSuccessfulCallback on_resource_load_successful);

  PrefetchProxyProxyingURLLoaderFactory(
      const PrefetchProxyProxyingURLLoaderFactory&) = delete;
  PrefetchProxyProxyingURLLoaderFactory& operator=(
      const PrefetchProxyProxyingURLLoaderFactory&) = delete;

  ~PrefetchProxyProxyingURLLoaderFactory() override;

  // Informs |this| that new subresource loads are being done after the user
  // clicked on a link that was previously prerendered. From this point on, all
  // requests for resources in |cached_subresources| will be done from
  // |isolated_factory_|'s cache and any other request will be done by
  // |network_process_factory_|.
  void NotifyPageNavigatedToAfterSRP(const std::set<GURL>& cached_subresources);

  // network::mojom::URLLoaderFactory:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory>
                 loader_receiver) override;

 private:
  class InProgressRequest : public network::mojom::URLLoader,
                            public network::mojom::URLLoaderClient {
   public:
    InProgressRequest(
        Profile* profile,
        PrefetchProxyProxyingURLLoaderFactory* parent_factory,
        network::mojom::URLLoaderFactory* target_factory,
        ResourceLoadSuccessfulCallback on_resource_load_successful,
        mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
        int32_t request_id,
        uint32_t options,
        const network::ResourceRequest& request,
        mojo::PendingRemote<network::mojom::URLLoaderClient> client,
        const net::MutableNetworkTrafficAnnotationTag& traffic_annotation);

    InProgressRequest(const InProgressRequest&) = delete;
    InProgressRequest& operator=(const InProgressRequest&) = delete;

    ~InProgressRequest() override;

    // Sets a callback that will be run during |OnComplete| to record metrics.
    using OnCompleteRecordMetricsCallback = base::OnceCallback<void(
        const GURL& url,
        network::mojom::URLResponseHeadPtr head,
        const network::URLLoaderCompletionStatus& status)>;
    void SetOnCompleteRecordMetricsCallback(
        OnCompleteRecordMetricsCallback callback);

    // network::mojom::URLLoader:
    void FollowRedirect(
        const std::vector<std::string>& removed_headers,
        const net::HttpRequestHeaders& modified_headers,
        const net::HttpRequestHeaders& modified_cors_exempt_headers,
        const absl::optional<GURL>& new_url) override;
    void SetPriority(net::RequestPriority priority,
                     int32_t intra_priority_value) override;
    void PauseReadingBodyFromNet() override;
    void ResumeReadingBodyFromNet() override;

    // network::mojom::URLLoaderClient:
    void OnReceiveEarlyHints(
        network::mojom::EarlyHintsPtr early_hints) override;
    void OnReceiveResponse(
        network::mojom::URLResponseHeadPtr head,
        mojo::ScopedDataPipeConsumerHandle body,
        absl::optional<mojo_base::BigBuffer> cached_metadata) override;
    void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                           network::mojom::URLResponseHeadPtr head) override;
    void OnUploadProgress(int64_t current_position,
                          int64_t total_size,
                          OnUploadProgressCallback callback) override;
    void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
    void OnComplete(const network::URLLoaderCompletionStatus& status) override;

   private:
    void OnBindingsClosed();

    // Runs |on_resource_load_successful_| for each url in |redirect_chain_| if
    // the resource was successfully loaded.
    void MaybeReportResourceLoadSuccess(
        const network::URLLoaderCompletionStatus& status);

    raw_ptr<Profile> profile_;

    // Back pointer to the factory which owns this class.
    const raw_ptr<PrefetchProxyProxyingURLLoaderFactory> parent_factory_;

    // Callback for recording metrics during |OnComplete|. Not always set.
    OnCompleteRecordMetricsCallback on_complete_metrics_callback_;

    // This should be run on destruction of |this|.
    base::OnceClosure destruction_callback_;

    // Holds onto the response head for reporting to the metrics callback.
    network::mojom::URLResponseHeadPtr head_;

    // All urls loaded by |this| in order of redirects. The first element is the
    // requested url and the last element is the final loaded url. Always has
    // length of at least 1.
    std::vector<GURL> redirect_chain_;

    // Used to report successfully loaded urls in the redirect chain.
    ResourceLoadSuccessfulCallback on_resource_load_successful_;

    // There are the mojo pipe endpoints between this proxy and the renderer.
    // Messages received by |client_receiver_| are forwarded to
    // |target_client_|.
    mojo::Remote<network::mojom::URLLoaderClient> target_client_;
    mojo::Receiver<network::mojom::URLLoader> loader_receiver_;

    // These are the mojo pipe endpoints between this proxy and the network
    // process. Messages received by |loader_receiver_| are forwarded to
    // |target_loader_|.
    mojo::Remote<network::mojom::URLLoader> target_loader_;
    mojo::Receiver<network::mojom::URLLoaderClient> client_receiver_{this};
  };

  // Terminates the request when constructed.
  class AbortRequest : public network::mojom::URLLoader {
   public:
    AbortRequest(
        mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
        mojo::PendingRemote<network::mojom::URLLoaderClient> client);

    AbortRequest(const AbortRequest&) = delete;
    AbortRequest& operator=(const AbortRequest&) = delete;

    ~AbortRequest() override;

    // network::mojom::URLLoader:
    void FollowRedirect(
        const std::vector<std::string>& removed_headers,
        const net::HttpRequestHeaders& modified_headers,
        const net::HttpRequestHeaders& modified_cors_exempt_headers,
        const absl::optional<GURL>& new_url) override;
    void SetPriority(net::RequestPriority priority,
                     int32_t intra_priority_value) override;
    void PauseReadingBodyFromNet() override;
    void ResumeReadingBodyFromNet() override;

   private:
    void OnBindingClosed();
    void Abort();

    // There are the mojo pipe endpoints between this proxy and the renderer.
    mojo::Remote<network::mojom::URLLoaderClient> target_client_;
    mojo::Receiver<network::mojom::URLLoader> loader_receiver_;

    base::WeakPtrFactory<AbortRequest> weak_factory_{this};
  };

  // Used as a callback for determining the eligibility of a resource to be
  // cached during prerender.
  void OnEligibilityResult(
      Profile* profile,
      mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      const GURL& url,
      bool eligible,
      absl::optional<PrefetchProxyPrefetchStatus> status);

  void RecordSubresourceMetricsDuringPrerender(
      const GURL& url,
      network::mojom::URLResponseHeadPtr head,
      const network::URLLoaderCompletionStatus& status);

  void RecordSubresourceMetricsAfterClick(
      const GURL& url,
      network::mojom::URLResponseHeadPtr head,
      const network::URLLoaderCompletionStatus& status);

  // Returns true when this factory was created during a NoStatePrefetch.
  // Internally, this means |NotifyPageNavigatedToAfterSRP| has not been called.
  bool ShouldHandleRequestForPrerender() const;

  void OnNetworkProcessFactoryError();
  void OnIsolatedFactoryError();
  void OnProxyBindingError();
  void RemoveRequest(InProgressRequest* request);
  void MaybeDestroySelf();

  // Must outlive |this|.
  raw_ptr<ResourceMetricsObserver> metrics_observer_;
  // For getting the web contents.
  const int frame_tree_node_id_;

  // When |previously_cached_subresources_| is set,
  // |NotifyPageNavigatedToAfterSRP| has been called and the behavior there will
  // take place using this set as the resources that can be loaded from cache.
  absl::optional<std::set<GURL>> previously_cached_subresources_;

  mojo::ReceiverSet<network::mojom::URLLoaderFactory> proxy_receivers_;

  // Passed to each InProgressRequest so they can report successfully loaded
  // urls in their redirect chain.
  ResourceLoadSuccessfulCallback on_resource_load_successful_;

  // All active network requests handled by this factory.
  std::set<std::unique_ptr<InProgressRequest>, base::UniquePtrComparator>
      requests_;

  // Tracks how many requests the prerender has made in order to limit the
  // number of subresources that can be prefetched by one page.
  size_t request_count_ = 0;

  // The network process URLLoaderFactory.
  mojo::Remote<network::mojom::URLLoaderFactory> network_process_factory_;

  // The isolated URLLoaderFactory.
  mojo::Remote<network::mojom::URLLoaderFactory> isolated_factory_;

  // Deletes |this| when run.
  DisconnectCallback on_disconnect_;

  base::WeakPtrFactory<PrefetchProxyProxyingURLLoaderFactory> weak_factory_{
      this};
};

#endif  // CHROME_BROWSER_PRELOADING_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_PROXYING_URL_LOADER_FACTORY_H_
