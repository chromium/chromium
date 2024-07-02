// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PREFETCH_SEARCH_PREFETCH_STREAMING_SEARCH_PREFETCH_URL_LOADER_H_
#define CHROME_BROWSER_PRELOADING_PREFETCH_SEARCH_PREFETCH_STREAMING_SEARCH_PREFETCH_URL_LOADER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_request.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_url_loader.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"

// This class starts a search prefetch and is able to serve it once headers are
// received. This allows streaming the response from memory as the response
// finishes from the network. The class drains the network request URL Loader,
// and creates a data pipe to handoff, so it may close the network URL Loader
// after the read from the network is done.
// The ownership of this instance is quite complicated. Its owners can be:
// - `SearchPrefetchRequest::streaming_url_loader_`: the request uses this
//   instance for fetching, and keeps an reference to the
//   `StreamingSearchPrefetchURLLoader` so as to allow SearchPrefetchService to
//   take the fetched responses.
// - `StreamingSearchPrefetchURLLoader::self_pointer_`: This instance is serving
//   to a real navigation, so it is owned by itself, and the Mojo connection
//   would manage its lifetime.
// - `StreamingSearchPrefetchURLLoader::ResponseReader::loader_`: This instance
//   is serving to a prerender navigation, so it  is owned by the ResponseReader
//   that is reading its response, and the Mojo connection would manage its
//   lifetime.
// To summarize, it can be:
// | Case            | owned by Request | Owned by self | Owned by Reader |
// | --------------- | ---------------- | ------------- | --------------- |
// | Not serving     | YES              | NO            | NO              |
// | ----------------|------------------|---------------|-----------------|
// | Serving to      |                  |               |                 |
// | real navigation | NO               | YES           | NO              |
// | ----------------|------------------|---------------|-----------------|
// | Serving to      |                  |               |                 |
// | prerender navi  | NO/YES *1        | NO            | YES             |
// | ----------------|------------------|---------------|-----------------|
// | Serving to      |                  |               |                 |
// | both navi *2    | NO               | YES           | YES             |
// *0: During serving, the references may be owned by callbacks in the
//     navigation stack. The pointer would be destroyed (once the callback is
//     destroyed) or transited to this instance/a reader soon.
// *1: It depends on whether the response has been deleted or not.
//     This is because, for example, if the response is about to expire, it
//     should still serve all content to the prerender navigation, as the
//     prerender navigation might be used by a real navigation.
// *2: Though it is also possible that the prerender navigation is used by a
//     real navigation, this case only consider the condition that
//     SearchPrefetchURLLoaderInterceptor directly intercepts a real navigation.
class StreamingSearchPrefetchURLLoader
    : public network::mojom::URLLoader,
      public network::mojom::URLLoaderClient,
      public SearchPrefetchURLLoader,
      public mojo::DataPipeDrainer::Client,
      public base::RefCounted<StreamingSearchPrefetchURLLoader> {
 public:
  // This enum is mainly for checking the correctness of reader serving.
  // These values are persisted to logs as SearchPreloadForwardingResult.
  // Entries should not be renumbered and numeric values should never be reused.
  enum class ForwardingResult {
    // This loader is not serving to any navigation via `forwarding_client`.
    // (Note: if the loader is served to prerendering reader, and the
    // prerendering navigation is activated, the status should also be
    // kNotServed, as `forwarding_client` did not participate in serving.)
    kNotServed = 0,
    // Set to this value when starting serving. Should not be recorded as a
    // terminate status.
    kStartedServing = 1,
    // Terminate status; Encountered errors while serving.
    kFailed = 2,
    // Terminate status; Successfully push the last byte.
    kCompleted = 3,
    kMaxValue = kCompleted,
  };

  // Used to reading the prefetched response from
  // `StreamingSearchPrefetchURLLoader`'s data cache.
  class ResponseReader : public network::mojom::URLLoader {
   public:
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused. Recorded as
    // SearchPrefetchResponseDataReaderStatus in logs.
    enum class ResponseDataReaderStatus {
      kCreated = 0,
      // This reader failed to push data to its clients. This is usually caused
      // by the clients refused to receive data after destruction.
      kServingError = 1,
      // The loader receives an error from the network and terminates this
      // reader.
      kNetworkError = 2,
      // For a success serving case.
      kCompleted = 3,
      // Its owner deletes this instance before this instance encounters any
      // issues or completes serving.
      kCanceledByLoader = 4,
      kMaxValue = kCanceledByLoader,
    };

    ResponseReader(
        mojo::PendingReceiver<network::mojom::URLLoader> forward_receiver,
        mojo::PendingRemote<network::mojom::URLLoaderClient> forwarding_client,
        base::OnceCallback<void(ResponseReader*)>
            forwarding_disconnection_callback,
        std::optional<network::URLLoaderCompletionStatus>,
        scoped_refptr<StreamingSearchPrefetchURLLoader> loader);
    ~ResponseReader() override;

    // Not copyable nor movable.
    ResponseReader(const ResponseReader&) = delete;
    ResponseReader& operator=(const ResponseReader&) = delete;
    ResponseReader(ResponseReader&&) = delete;
    ResponseReader& operator=(ResponseReader&&) = delete;

    // network::mojom::URLLoader implementation:
    void FollowRedirect(
        const std::vector<std::string>& removed_headers,
        const net::HttpRequestHeaders& modified_headers,
        const net::HttpRequestHeaders& modified_cors_exempt_headers,
        const std::optional<GURL>& new_url) override;
    void SetPriority(net::RequestPriority priority,
                     int32_t intra_priority_value) override;
    void PauseReadingBodyFromNet() override;
    void ResumeReadingBodyFromNet() override;

    // Sets date pipe up between this instance and its client.
    void StartReadingResponseFromData(
        network::mojom::URLResponseHeadPtr& resource_response);

    // TODO(crbug.com/40250486): These methods will replace the
    // `StreamingSearchPrefetchURLLoader`'s.
    // Pushes the received data into the producer end of the data pipe.
    void PushData();
    // `PushData` may find itself having to wait until the data pipe to
    // be ready. At that time, this method would be invoked.
    void OnDataHandleReady(MojoResult result,
                           const mojo::HandleSignalsState& state);

    // Called by `StreamingSearchPrefetchURLLoader` to inform it of the
    // received response.
    void OnStatusCodeReady(const network::URLLoaderCompletionStatus& status);
    void OnDestroyed();

   private:
    // Checks if all data have be pushed to its consumer and the corresponding
    // loader has completed fetching. If so, inform the forwarding client.
    void MaybeSendCompletionSignal();

    // Called upon the forwarding Mojo pipeline disconnection. After this point,
    // `this` no longer needs to keep a reference to
    // `StreamingSearchPrefetchURLLoader`.
    void OnForwardingDisconnection();

    void ReleaseSelfReference();

    // Set to true once it writes all bytes into the data pipe.
    bool complete_writing_ = false;

    // Records the position where to read the next body from.
    // content-----------------------------
    //            ^                       ^
    //   read     |  to be read.          |
    //            write_position_         end of response body.
    int write_position_ = 0;

    // Tracking the current status.
    ResponseDataReaderStatus status_ = ResponseDataReaderStatus::kCreated;

    // Data pipe for pushing the received response to the client.
    mojo::ScopedDataPipeProducerHandle producer_handle_;
    std::unique_ptr<mojo::SimpleWatcher> handle_watcher_;

    // Forwarding prefetched response to another loader.
    mojo::Receiver<network::mojom::URLLoader> forwarding_receiver_{this};
    mojo::Remote<network::mojom::URLLoaderClient> forwarding_client_;

    // Invoked when `this` does not need more data from the loader and asks the
    // loader to delete itself.
    base::OnceCallback<void(ResponseReader*)> disconnection_callback_;

    // Refer to the corresponding loader that is fetching cache for this reader.
    // Release on `forwarding_receiver_` losing connection.
    // Note that there can be a reference cycle, where `loader_` owns `this` and
    // `this` owns `loader_`. But it should be safe, as the destruction order
    // should be:
    // 1. `this` asks `loader_` to post a task A to delete `this`.
    // 2. `this` posts a task B to release the reference pointer to
    //    `StreamingSearchPrefetchURLLoader`.
    // After this point `this` no longer owns
    // `StreamingSearchPrefetchURLLoader`. So, `loader_` should always be
    // deleted asynchronously.
    scoped_refptr<StreamingSearchPrefetchURLLoader> loader_;

    // Records the completion status for the corresponding network loader.
    std::optional<network::URLLoaderCompletionStatus>
        url_loader_completion_status_;

    // TODO(crbug.com/40250486): We'd have a failure strategy to determine
    // whether to fallback real navigation or to discard the reader's caller.
  };

  // Creates a network service URLLoader, binds to the URL Loader, and starts
  // the request.
  StreamingSearchPrefetchURLLoader(
      SearchPrefetchRequest* streaming_prefetch_request,
      Profile* profile,
      bool navigation_prefetch,
      std::unique_ptr<network::ResourceRequest> resource_request,
      const net::NetworkTrafficAnnotationTag& network_traffic_annotation,
      base::OnceCallback<void(bool)> report_error_callback);

  // Returns a callback which can connect a navigation request with this
  // instance, and the request can read `this`'s received response.
  static RequestHandler GetCallbackForReadingViaResponseReader(
      scoped_refptr<StreamingSearchPrefetchURLLoader> loader);

  // Similar to `GetCallbackForReadingViaResponseReader`, but support direct
  // forwarding.
  // TODO(crbug.com/40250486): Unify the logic and delete this entry.
  static RequestHandler GetServingResponseHandler(
      scoped_refptr<StreamingSearchPrefetchURLLoader> loader);

  // Clears |streaming_prefetch_request_|, which initially owns |this|. Once
  // this is cleared, the class is self managed and needs to delete itself based
  // on mojo channels closing or other errors occurring.
  void ClearOwnerPointer();

  // Record whether the navigation url and the |prefetch_url_| match. Only
  // recorded when |navigation_prefetch_| is true.
  void RecordNavigationURLHistogram(const GURL& navigation_url);

  void set_on_destruction_callback_for_testing(
      base::OnceClosure on_destruction_callback_for_testing) {
    on_destruction_callback_for_testing_ =
        std::move(on_destruction_callback_for_testing);
  }

 private:
  friend class base::RefCounted<StreamingSearchPrefetchURLLoader>;

  // Hide its destructor.
  ~StreamingSearchPrefetchURLLoader() override;

  // mojo::DataPipeDrainer::Client:
  void OnDataAvailable(base::span<const uint8_t> data) override;
  void OnDataComplete() override;

  // network::mojom::URLLoader:
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const std::optional<GURL>& new_url) override;
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  // network::mojom::URLLoaderClient
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override;
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr head,
      mojo::ScopedDataPipeConsumerHandle body,
      std::optional<mojo_base::BigBuffer> cached_metadata) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         network::mojom::URLResponseHeadPtr head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback callback) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

  // When a disconnection occurs in the network URLLoader mojo pipe, this
  // object's lifetime needs to be managed and the connections need to be closed
  // unless complete has happened. This is the prefetch pathway callback.
  void OnURLLoaderMojoDisconnect();

  // When a disconnection occurs in the network URLLoader mojo pipe, this
  // object's lifetime needs to be managed and the connections need to be closed
  // unless complete has happened. This is the fallback pathway callback.
  void OnURLLoaderMojoDisconnectInFallback();

  // When a disconnection occurs in the navigation client mojo pipe, this
  // object's lifetime needs to be managed and the connections need to be
  // closed.
  void OnURLLoaderClientMojoDisconnect();

  // When the given `reader` asks `this` to delete itself. If the prerendering
  // navigation is the only one that needs this loader, delete the loader.
  void OnPrerenderForwardingDisconnect(ResponseReader* reader);

  // Start serving the response from |producer_handle_|, which serves
  // |body_content_|.
  void OnStartLoadingResponseBodyFromData();

  // Called when more data can be sent into |producer_handle_|.
  void OnHandleReady(MojoResult result, const mojo::HandleSignalsState& state);

  // Returns the view of `body_content_`, starting from the `writing_position`
  // and ending at the end of the string.
  // Returns an invalid std::string_view (note, not an empty std::string_view)
  // if there is no more valid data. Returns an empty std::string_view if
  // writing_position reaches the end of the current response body but `this` is
  // waiting for the network to produce more data.
  std::string_view GetMoreDataFromCache(size_t writing_position) const;

  // Push data into |producer_handle_|.
  void PushData();

  // Clears |producer_handle_| and |handle_watcher_|.
  void Finish();

  // Post a task to release the self ownership at a later point.
  // Note that it cannot guarantee this instance will be destroyed, as
  // ResponseReader can own this instance.
  void PostTaskToReleaseOwnership();

  // Creates a default network URL loader for the original request.
  void Fallback();

  // Sets up mojo forwarding to the navigation path. Resumes
  // |network_url_loader_| calls. Serves the start of the response to the
  // navigation path. After this method is called, |this| manages its own
  // lifetime by owning `self_pointer_`, which is a scoped_refptr to `this`,
  // and the reference will be released once the mojo connection is
  // disconnected.
  void SetUpForwardingClient(
      const network::ResourceRequest&,
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> forwarding_client);

  // Similar to `SetUpForwardingClient`, but sets up mojo forwarding to
  // the prerender navigation path.
  void CreateResponseReaderForPrerender(
      const network::ResourceRequest& resource_request,
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> forwarding_client);

  // Forwards all queued events to |forwarding_client_|.
  void RunEventQueue();

  // Marks the parent prefetch request as servable. Called as delayed task.
  void MarkPrefetchAsServable();

  // Called on `this` receives servable response.
  void OnServableResponseCodeReceived();

  // The network URLLoader that fetches the prefetch URL and its receiver.
  mojo::Remote<network::mojom::URLLoader> network_url_loader_;
  mojo::Receiver<network::mojom::URLLoaderClient> url_loader_receiver_{this};

  // Once the prefetch response is received and is ready to be served, the
  // response info related to the request. When this becomes populated, the
  // network URL Loader calls are paused.
  network::mojom::URLResponseHeadPtr resource_response_;

  // The request that is being prefetched.
  std::unique_ptr<network::ResourceRequest> resource_request_;

  // The initiating prefetch request. Cleared when handing this request off to
  // the navigation stack.
  raw_ptr<SearchPrefetchRequest, FlakyDanglingUntriaged>
      streaming_prefetch_request_;

  // Whether we are serving from |body_content_|.
  bool serving_from_data_ = false;

  // The status returned from |network_url_loader_|.
  std::optional<network::URLLoaderCompletionStatus> status_;

  // Bytes sent to |producer_handle_| already.
  size_t write_position_ = 0;
  // The request body.
  std::string body_content_;
  int estimated_length_ = 0;
  // Whether the body has fully been drained from |network_url_loader_|.
  bool drain_complete_ = false;
  // Drainer for the content in |network_url_loader_|.
  std::unique_ptr<mojo::DataPipeDrainer> pipe_drainer_;

  // Once `forwarding_client_` is set, this status tracks the whether the
  // forwarding is successfully completed.
  ForwardingResult forwarding_result_ = ForwardingResult::kNotServed;

  // URL Loader Events that occur before serving to the navigation stack should
  // be queued internally until the request is being served.
  std::vector<base::OnceClosure> event_queue_;
  // TODO(crbug.com/40250486): Migrate `receiver_`, `forwarding_client_`,
  // `producer_handle_`, `handle_watcher_` and `write_position_` into
  // ResponseReader.
  // Forwarding client receiver.
  mojo::Receiver<network::mojom::URLLoader> receiver_{this};
  mojo::Remote<network::mojom::URLLoaderClient> forwarding_client_;

  // DataPipe for forwarding the stored response body `body_content_` to the
  // forwarding client.
  mojo::ScopedDataPipeProducerHandle producer_handle_;
  std::unique_ptr<mojo::SimpleWatcher> handle_watcher_;

  // Set when it is managing its own lifetime. Should be released when
  // `receiver_` is disconnected or encountered a failure.
  scoped_refptr<StreamingSearchPrefetchURLLoader> self_pointer_;

  // TODO(crbug.com/40250486): Make it a generic reader.
  std::unique_ptr<ResponseReader> response_reader_for_prerender_;
  // The number of times that this loader created a reader and served the
  // response to prerendering navigation.
  int count_prerender_serving_times_ = 0;

  // Set to true when we encounter an error in between when the prefetch request
  // owns this loader and the loader has started. When the forwarding client is
  // set up, we will delete soon |this|.
  bool pending_delete_ = false;

  // Whether fallback should start when search prefetch is activated (i.e., when
  // `is_activated_` is set).
  bool is_scheduled_to_fallback_ = false;

  // Whether fallback has started.
  bool is_in_fallback_ = false;

  // If the navigation path paused the url loader. Used to pause the network url
  // loader on fallback.
  bool paused_ = false;

  // Whenever an error is reported, it needs to be reported to the service via
  // this callback.
  base::OnceCallback<void(bool)> report_error_callback_;

  // Track if the request has already been marked as servable, and if so, don't
  // report it again.
  bool marked_as_servable_ = false;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  net::NetworkTrafficAnnotationTag network_traffic_annotation_;

  // Whether this loader is created specifically for a navigation prefetch.
  bool navigation_prefetch_;

  // The prefetch URL, used to record whether the prefetch and navigation URLs
  // match when this is a navigation prefetch.
  GURL prefetch_url_;

  // Whether this url loader was activated via the navigation stack.
  bool is_activated_ = false;

  base::OnceClosure on_destruction_callback_for_testing_;

  base::WeakPtrFactory<StreamingSearchPrefetchURLLoader> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PRELOADING_PREFETCH_SEARCH_PREFETCH_STREAMING_SEARCH_PREFETCH_URL_LOADER_H_
