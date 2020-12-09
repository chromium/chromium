// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_FROM_STRING_URL_LOADER_H_
#define CHROME_BROWSER_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_FROM_STRING_URL_LOADER_H_

#include <cstdint>
#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/prefetch/search_prefetch/prefetched_response_container.h"
#include "chrome/browser/prefetch/search_prefetch/search_prefetch_url_loader.h"
#include "content/public/browser/url_loader_request_interceptor.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace mojo {
class SimpleWatcher;
}

namespace net {
class StringIOBuffer;
}

class SearchPrefetchFromStringURLLoader : public network::mojom::URLLoader,
                                          public SearchPrefetchURLLoader {
 public:
  explicit SearchPrefetchFromStringURLLoader(
      std::unique_ptr<PrefetchedResponseContainer> response);

  ~SearchPrefetchFromStringURLLoader() override;

  SearchPrefetchFromStringURLLoader(const SearchPrefetchFromStringURLLoader&) =
      delete;
  SearchPrefetchFromStringURLLoader& operator=(
      const SearchPrefetchFromStringURLLoader&) = delete;

  // SearchPrefetchURLLoader:
  SearchPrefetchURLLoader::RequestHandler ServingResponseHandler(
      std::unique_ptr<SearchPrefetchURLLoader> loader) override;

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

  // Binds |this| to the mojo handlers and starts the network request using
  // |request|. After this method is called, |this| manages its own lifetime;
  // |loader| points to |this| and can be released once the mojo connection is
  // set up.
  void BindAndStart(
      std::unique_ptr<SearchPrefetchURLLoader> loader,
      const network::ResourceRequest& request,
      mojo::PendingReceiver<network::mojom::URLLoader> url_loader_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> forwarding_client);

  // Called when the mojo handle's state changes, either by being ready for more
  // data or an error.
  void OnHandleReady(MojoResult result, const mojo::HandleSignalsState& state);

  // Finishes the request with the given net error.
  void Finish(int error);

  // Sends data on the mojo pipe.
  void TransferRawData();

  // Unbinds and deletes |this|.
  void OnMojoDisconnect();

  // Deletes |this| if it is not bound to the mojo pipes.
  void MaybeDeleteSelf();

  // The response that will be sent to mojo.
  network::mojom::URLResponseHeadPtr head_;
  scoped_refptr<net::StringIOBuffer> body_buffer_;

  // Keeps track of the position of the data transfer.
  int write_position_ = 0;

  // The length of |body_buffer_|.
  const int bytes_of_raw_data_to_transfer_ = 0;

  // Mojo plumbing.
  mojo::Receiver<network::mojom::URLLoader> receiver_{this};
  mojo::Remote<network::mojom::URLLoaderClient> client_;
  mojo::ScopedDataPipeProducerHandle producer_handle_;
  std::unique_ptr<mojo::SimpleWatcher> handle_watcher_;

  base::WeakPtrFactory<SearchPrefetchFromStringURLLoader> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_FROM_STRING_URL_LOADER_H_
