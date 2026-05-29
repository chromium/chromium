// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_URL_LOADER_THROTTLE_H_
#define CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_URL_LOADER_THROTTLE_H_

#include "base/supports_user_data.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace signin {

class HeaderModificationDelegate;

// This class is used to modify the main frame request made when loading the
// GAIA signin realm.
class URLLoaderThrottle : public blink::URLLoaderThrottle,
                          public base::SupportsUserData {
 public:
  // Creates a new throttle if |delegate| says that this request should be
  // intercepted.
  static std::unique_ptr<URLLoaderThrottle> MaybeCreate(
      std::unique_ptr<HeaderModificationDelegate> delegate,
      content::WebContents::Getter web_contents_getter);

  URLLoaderThrottle(const URLLoaderThrottle&) = delete;
  URLLoaderThrottle& operator=(const URLLoaderThrottle&) = delete;

  ~URLLoaderThrottle() override;

  // blink::URLLoaderThrottle
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;
  void WillRedirectRequest(
      net::RedirectInfo* redirect_info,
      const network::mojom::URLResponseHead& response_head,
      bool* defer,
      std::vector<std::string>* headers_to_remove,
      net::HttpRequestHeaders* modified_headers,
      net::HttpRequestHeaders* modified_cors_exempt_headers) override;
  void WillProcessResponse(const GURL& response_url,
                           network::mojom::URLResponseHead* response_head,
                           bool* defer) override;

 private:
  class ThrottleRequestAdapter;
  class ThrottleResponseAdapter;

  URLLoaderThrottle(std::unique_ptr<HeaderModificationDelegate> delegate,
                    content::WebContents::Getter web_contents_getter);

  const std::unique_ptr<HeaderModificationDelegate> delegate_;
  const content::WebContents::Getter web_contents_getter_;

  // Information about the current request.
  GURL request_url_;
  // Refers to the "last" referrer in the redirect chain.
  GURL request_referrer_;
  // The origin that initiated the request. May be empty for browser-initiated
  // requests. See network::ResourceRequest::request_initiator for details.
  std::optional<url::Origin> request_initiator_;
  std::optional<url::Origin> request_top_frame_origin_;
  net::HttpRequestHeaders request_headers_;
  net::HttpRequestHeaders request_cors_exempt_headers_;
  network::mojom::RequestDestination request_destination_ =
      network::mojom::RequestDestination::kEmpty;
  bool is_outermost_main_frame_ = false;
  bool request_is_fetch_like_api_ = false;

  base::OnceClosure destruction_callback_;
};

}  // namespace signin

#endif  // CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_URL_LOADER_THROTTLE_H_
