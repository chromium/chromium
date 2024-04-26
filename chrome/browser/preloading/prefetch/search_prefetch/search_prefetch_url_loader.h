// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_URL_LOADER_H_
#define CHROME_BROWSER_PRELOADING_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_URL_LOADER_H_

#include "base/time/time.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"

// A simple interface that defines the behavior that a search prefetch URL
// loader must provide.
// Basically, it should implement a method that takes its pointer as the input
// `loader`, and return `loader` which is the owning pointer to `this`.
// That method will be called when the response should be served to the user.
// `loader` needs to be stored within the returned `RequestHandler` to allow
// `this` to be owned by the callback. This allows ownership until the callback
// is run, which then should have ownership owned via a mojo connection.
// Since its implementations use different ownership model, it does not define
// virtual functions for creating `RequestHandler`. See
// StreamingSearchPrefetchURLLoader::GetCallbackForReadingViaResponseReader as
// an example.
// TODO(crbug.com/40250486): Remove this class.
class SearchPrefetchURLLoader {
 public:
  virtual ~SearchPrefetchURLLoader() = default;
  using RequestHandler = base::OnceCallback<void(
      const network::ResourceRequest& resource_request,
      mojo::PendingReceiver<network::mojom::URLLoader> url_loader_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client)>;

 protected:
  void RecordInterceptionTime();

  void OnForwardingComplete();

 private:
  base::TimeTicks interception_time_;
};

#endif  // CHROME_BROWSER_PRELOADING_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_URL_LOADER_H_
