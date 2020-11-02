// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_URL_LOADER_H_
#define CHROME_BROWSER_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_URL_LOADER_H_

#include <stdint.h>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/url_loader_request_interceptor.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"

// An URL loader that allows access to otherwise enclosed state like the
// URLLoaderFactory for prefetch proxy.
class PrefetchProxyURLLoader : public network::mojom::URLLoader,
                               public network::mojom::URLLoaderClient {
 public:
  using RequestHandler = base::OnceCallback<void(
      const network::ResourceRequest& resource_request,
      mojo::PendingReceiver<network::mojom::URLLoader> url_loader_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client)>;

  // The ResourceRequest is unused since it has not been acted on by
  // URLLoaderThrottles yet.
  PrefetchProxyURLLoader(const network::ResourceRequest& unused_request,
                         const scoped_refptr<network::SharedURLLoaderFactory>&
                             network_loader_factory,
                         int32_t routing_id,
                         int32_t request_id);
  ~PrefetchProxyURLLoader() override;

  // Called when the response should be fetched in an isolated manner. Returns a
  // handler.
  RequestHandler ServingResponseHandler();

  // Binds |this| to the mojo handlers and starts the network request using
  // |request|. After this method is called, |this| manages its own lifetime.
  void BindAndStart(
      const network::ResourceRequest& request,
      mojo::PendingReceiver<network::mojom::URLLoader> url_loader_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> forwarding_client);

 private:
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
  // network::mojom::URLLoaderClient:
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

  // When a connection error occurs in either mojo pipe, this objects lifetime
  // needs to be managed and the connections need to be closed.
  void OnConnectionError();

  // Kept as members since they are set in the constructor but needed in
  // |BindAndStart|.
  const scoped_refptr<network::SharedURLLoaderFactory> network_loader_factory_;
  const int32_t routing_id_;
  const int32_t request_id_;

  // The network URLLoader that fetches the request from the network and its
  // client.
  mojo::Remote<network::mojom::URLLoader> network_loader_;
  mojo::Receiver<network::mojom::URLLoaderClient> network_loader_client_{this};

  // Forwarding client receiver.
  mojo::Receiver<network::mojom::URLLoader> receiver_{this};
  mojo::Remote<network::mojom::URLLoaderClient> forwarding_client_;

  base::WeakPtrFactory<PrefetchProxyURLLoader> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PrefetchProxyURLLoader);
};

#endif  // CHROME_BROWSER_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_URL_LOADER_H_
