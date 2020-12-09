// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_URL_LOADER_H_
#define CHROME_BROWSER_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_URL_LOADER_H_

#include <memory>

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

// A simple interface that defines the behavior that a search prefetch URL
// loader must provide.
class SearchPrefetchURLLoader {
 public:
  virtual ~SearchPrefetchURLLoader() = default;

  using RequestHandler = base::OnceCallback<void(
      const network::ResourceRequest& resource_request,
      mojo::PendingReceiver<network::mojom::URLLoader> url_loader_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client)>;

  // Called when the response should be served to the user. Returns a handler.
  // |loader| is the owning pointer to |this|. It needs to be stored within the
  // returned |RequestHandler| to allow |this| to be owned by the callback.
  // This allows ownership until the callback is run, which then should have
  // ownership owned via a mojo connection.
  virtual RequestHandler ServingResponseHandler(
      std::unique_ptr<SearchPrefetchURLLoader> loader) = 0;
};

#endif  // CHROME_BROWSER_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_URL_LOADER_H_
