// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFETCH_SEARCH_PREFETCH_STREAMING_SEARCH_PREFETCH_URL_LOADER_H_
#define CHROME_BROWSER_PREFETCH_SEARCH_PREFETCH_STREAMING_SEARCH_PREFETCH_URL_LOADER_H_

#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/prefetch/search_prefetch/search_prefetch_url_loader.h"
#include "chrome/browser/prefetch/search_prefetch/streaming_search_prefetch_request.h"
#include "content/public/browser/url_loader_request_interceptor.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "services/network/public/mojom/url_loader.mojom-forward.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"

// This class starts a search prefetch and is able to serve it once headers are
// received. This allows streaming the response from memory as the response
// finishes from the network. The class drains the network request URL Loader,
// and creates a data pipe to handoff, so it may close the network URL Loader
// after the read from the network is done.
class StreamingSearchPrefetchURLLoader : public network::mojom::URLLoader,
                                         public network::mojom::URLLoaderClient,
                                         public SearchPrefetchURLLoader,
                                         public mojo::DataPipeDrainer::Client {
 public:
  // Creates a network service URLLoader, binds to the URL Loader, and starts
  // the request.
  StreamingSearchPrefetchURLLoader(
      StreamingSearchPrefetchRequest* streaming_prefetch_request,
      Profile* profile,
      std::unique_ptr<network::ResourceRequest> resource_request,
      const net::NetworkTrafficAnnotationTag& network_traffic_annotation);

  ~StreamingSearchPrefetchURLLoader() override;

  // Clears |streaming_prefetch_request_|, which initially owns |this|. Once
  // this is cleared, the class is self managed and needs to delete itself based
  // on mojo channels closing or other errors occurring.
  void ClearOwnerPointer();

 private:
  // mojo::DataPipeDrainer::Client:
  void OnDataAvailable(const void* data, size_t num_bytes) override;
  void OnDataComplete() override;

  // SearchPrefetchURLLoader:
  SearchPrefetchURLLoader::RequestHandler ServingResponseHandler(
      std::unique_ptr<SearchPrefetchURLLoader> loader) override;

  // network::mojom::URLLoader:
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const base::Optional<GURL>& new_url) override;
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  // network::mojom::URLLoaderClient
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override;
  void OnReceiveResponse(network::mojom::URLResponseHeadPtr head) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         network::mojom::URLResponseHeadPtr head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback callback) override;
  void OnReceiveCachedMetadata(mojo_base::BigBuffer data) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

  // When a disconnection occurs in the network URLLoader mojo pipe, this
  // object's lifetime needs to be managed and the connections need to be closed
  // unless complete has happened.
  void OnURLLoaderMojoDisconnect();

  // When a disconnection occurs in the navigation client mojo pipe, this
  // object's lifetime needs to be managed and the connections need to be
  // closed.
  void OnURLLoaderClientMojoDisconnect();

  // Start serving the response from |producer_handle_|, which serves
  // |body_content_|.
  void OnStartLoadingResponseBodyFromData();

  // Called when more data can be sent into |producer_handle_|.
  void OnHandleReady(MojoResult result, const mojo::HandleSignalsState& state);

  // Push data into |producer_handle_|.
  void PushData();

  // Clears |producer_handle_| and |handle_watcher_|.
  void Finish();

  // Sets up mojo forwarding to the navigation path. Resumes
  // |network_url_loader_| calls. Serves the start of the response to the
  // navigation path. After this method is called, |this| manages its own
  // lifetime; |loader| points to |this| and can be released once the mojo
  // connection is set up.
  void SetUpForwardingClient(
      std::unique_ptr<SearchPrefetchURLLoader> loader,
      const network::ResourceRequest&,
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> forwarding_client);

  // Forwards all queued events to |forwarding_client_|.
  void RunEventQueue();

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
  StreamingSearchPrefetchRequest* streaming_prefetch_request_;

  // Whether we are serving from |bdoy_content_|.
  bool serving_from_data_ = false;

  // The status returned from |network_url_loader_|.
  base::Optional<network::URLLoaderCompletionStatus> status_;

  // Total amount of bytes to transfer.
  int bytes_of_raw_data_to_transfer_ = 0;
  // Bytes sent to |producer_handle_| already.
  int write_position_ = 0;
  // The request body.
  std::string body_content_;
  int estimated_length_ = 0;
  // Whether the body has fully been drained from |network_url_loader_|.
  bool drain_complete_ = false;
  // Drainer for the content in |network_url_loader_|.
  std::unique_ptr<mojo::DataPipeDrainer> pipe_drainer_;

  // URL Loader Events that occur before serving to the navigation stack should
  // be queued internally until the request is being served.
  std::vector<base::OnceClosure> event_queue_;

  // Forwarding client receiver.
  mojo::Receiver<network::mojom::URLLoader> receiver_{this};
  mojo::Remote<network::mojom::URLLoaderClient> forwarding_client_;

  mojo::ScopedDataPipeProducerHandle producer_handle_;
  std::unique_ptr<mojo::SimpleWatcher> handle_watcher_;

  base::WeakPtrFactory<StreamingSearchPrefetchURLLoader> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PREFETCH_SEARCH_PREFETCH_STREAMING_SEARCH_PREFETCH_URL_LOADER_H_
