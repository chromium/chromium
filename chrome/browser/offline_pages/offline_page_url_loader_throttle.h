// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_OFFLINE_PAGE_URL_LOADER_THROTTLE_H_
#define CHROME_BROWSER_OFFLINE_PAGES_OFFLINE_PAGE_URL_LOADER_THROTTLE_H_

#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace offline_pages {

// Strips the internal `X-Chrome-offline` request header before a request is
// dispatched to the network or followed across a redirect.
//
// Registered after `OfflinePageURLLoaderRequestInterceptor` on the initial
// navigation leg so `OfflinePageURLLoader` still observes `X-Chrome-offline`
// for local archive lookup, while any request that falls back to the network
// (`FallbackToDefault()`), restarts (e.g. `ACCEPT_CH`), or redirects removes
// `X-Chrome-offline` before outbound dispatch. Complements redirect-time
// stripping in `OfflinePageTabHelper` and `OfflinePageURLLoader`.
class OfflinePageURLLoaderThrottle : public blink::URLLoaderThrottle {
 public:
  OfflinePageURLLoaderThrottle();
  OfflinePageURLLoaderThrottle(const OfflinePageURLLoaderThrottle&) = delete;
  OfflinePageURLLoaderThrottle& operator=(const OfflinePageURLLoaderThrottle&) =
      delete;
  ~OfflinePageURLLoaderThrottle() override;

  // blink::URLLoaderThrottle:
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;
  void WillRedirectRequest(
      net::RedirectInfo* redirect_info,
      const network::mojom::URLResponseHead& response_head,
      bool* defer,
      network::HttpRequestHeadersUpdateParams* headers_update_params) override;
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_OFFLINE_PAGE_URL_LOADER_THROTTLE_H_
