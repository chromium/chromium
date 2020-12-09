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
#include "services/network/public/mojom/url_loader.mojom-forward.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"

// This class starts a search prefetch and is able to serve it once headers are
// received. This allows streaming the response from memory as the response
// finishes from the network.
class StreamingSearchPrefetchURLLoader : public network::mojom::URLLoader,
                                         public network::mojom::URLLoaderClient,
                                         public SearchPrefetchURLLoader {
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

  // When a disconnection occurs in either mojo pipe, this object's lifetime
  // needs to be managed and the connections need to be closed.
  void OnMojoDisconnect();

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

  // Forwarding client receiver.
  mojo::Receiver<network::mojom::URLLoader> receiver_{this};
  mojo::Remote<network::mojom::URLLoaderClient> forwarding_client_;

  base::WeakPtrFactory<StreamingSearchPrefetchURLLoader> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PREFETCH_SEARCH_PREFETCH_STREAMING_SEARCH_PREFETCH_URL_LOADER_H_
