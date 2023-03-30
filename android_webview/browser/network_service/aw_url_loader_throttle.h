// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_NETWORK_SERVICE_AW_URL_LOADER_THROTTLE_H_
#define ANDROID_WEBVIEW_BROWSER_NETWORK_SERVICE_AW_URL_LOADER_THROTTLE_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "url/origin.h"

class GURL;

namespace net {
class HttpRequestHeaders;
}

namespace android_webview {
class AwBrowserContext;

class AwURLLoaderThrottle : public blink::URLLoaderThrottle {
 public:
  explicit AwURLLoaderThrottle(AwBrowserContext* aw_browser_context);

  AwURLLoaderThrottle(const AwURLLoaderThrottle&) = delete;
  AwURLLoaderThrottle& operator=(const AwURLLoaderThrottle&) = delete;

  ~AwURLLoaderThrottle() override;

  // blink::URLLoaderThrottle implementation:
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;
  void WillRedirectRequest(
      net::RedirectInfo* redirect_info,
      const network::mojom::URLResponseHead& response_head,
      bool* defer,
      std::vector<std::string>* to_be_removed_request_headers,
      net::HttpRequestHeaders* modified_request_headers,
      net::HttpRequestHeaders* modified_cors_exempt_request_headers) override;

 private:
  void AddExtraHeadersIfNeeded(const GURL& url,
                               net::HttpRequestHeaders* headers);

  raw_ptr<AwBrowserContext> aw_browser_context_;
  std::vector<std::string> added_headers_;
  url::Origin original_origin_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_NETWORK_SERVICE_AW_URL_LOADER_THROTTLE_H_
