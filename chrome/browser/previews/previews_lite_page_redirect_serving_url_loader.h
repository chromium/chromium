// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_REDIRECT_SERVING_URL_LOADER_H_
#define CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_REDIRECT_SERVING_URL_LOADER_H_

#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "content/public/browser/url_loader_request_interceptor.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace previews {

enum class ServingLoaderResult {
  kSuccess,   // The Preview can be served.
  kFallback,  // The Preview cannot be served and fallback to default URLLoader
              // should occur.
  kRedirect,  // The URL returns a redirect, and we should pass that on to the
              // navigation code.
};

using ResultCallback =
    base::OnceCallback<void(ServingLoaderResult,
                            base::Optional<net::RedirectInfo> redirect_info,
                            scoped_refptr<network::ResourceResponse> response)>;

using RequestHandler = base::OnceCallback<void(
    const network::ResourceRequest& resource_request,
    mojo::PendingReceiver<network::mojom::URLLoader>,
    mojo::PendingRemote<network::mojom::URLLoaderClient>)>;

// This class attempts to fetch a LitePage from the LitePage server, and if
// successful, calls a success callback. Otherwise, it calls fallback in the
// case of a failure and redirect in the case of a redirect served from the lite
// pages service.
class PreviewsLitePageRedirectServingURLLoader
    : public network::mojom::URLLoader,
      public network::mojom::URLLoaderClient {
 public:
  // Creates a network service URLLoader, binds to the URL Loader, and stores
  // the various callbacks.
  explicit PreviewsLitePageRedirectServingURLLoader(
      ResultCallback result_callback);
  ~PreviewsLitePageRedirectServingURLLoader() override;

  // Begins the underlying network URLLoader to fetch the preview.
  // |network_loader_factory| creates the URLLoader using the other parameters
  // in this method.
  void StartNetworkRequest(const network::ResourceRequest& request,
                           const scoped_refptr<network::SharedURLLoaderFactory>&
                               network_loader_factory,
                           int frame_tree_node_id);

  // Called when the response should be served to the user. Returns a handler.
  RequestHandler ServingResponseHandler();

  // network::mojom::URLLoader:
  void FollowRedirect(const std::vector<std::string>& removed_headers,
                      const net::HttpRequestHeaders& modified_headers,
                      const base::Optional<GURL>& new_url) override;
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

 private:
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

  // Calls |result_callback_| with kFallback and cleans up.
  void Fallback();

  // Calls timeout if the result was not previously determined (i.e., the
  // response has not started yet). This method is called
  // |LitePagePreviewsNavigationTimeoutDuration()| after the network request has
  // started.
  void Timeout();

  // Sets up mojo forwarding to the navigation path. Resumes
  // |network_url_loader_| calls. Serves the start of the response to the
  // navigation path.
  void SetUpForwardingClient(
      const network::ResourceRequest&,
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> forwarding_client);

  // The network URLLoader that fetches the LitePage URL and its receiver.
  network::mojom::URLLoaderPtr network_url_loader_;
  mojo::Receiver<network::mojom::URLLoaderClient> url_loader_receiver_{this};

  // When a result is determined, this callback should be run with the
  // appropriate ServingLoaderResult.
  ResultCallback result_callback_;

  // Once the LitePage response is received and is ready to be served, the
  // response info related to the request. When this becomes populated, the
  // network URL Loader calls are paused.
  scoped_refptr<network::ResourceResponse> resource_response_;

  // The frame tree node id associated with the request. Used to get the
  // BrowserContext on the UI thread for the request.
  int frame_tree_node_id_ = 0;

  // The previews URL that is being requested.
  GURL previews_url_;

  // The timer that triggers a timeout when the request takes too long.
  base::OneShotTimer timeout_timer_;

  // Forwarding client binding.
  mojo::Binding<network::mojom::URLLoader> binding_;
  mojo::Remote<network::mojom::URLLoaderClient> forwarding_client_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PreviewsLitePageRedirectServingURLLoader>
      weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PreviewsLitePageRedirectServingURLLoader);
};

}  // namespace previews

#endif  // CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_REDIRECT_SERVING_URL_LOADER_H_
