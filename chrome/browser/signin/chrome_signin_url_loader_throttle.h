// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_URL_LOADER_THROTTLE_H_
#define CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_URL_LOADER_THROTTLE_H_

#include "base/macros.h"
#include "base/supports_user_data.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/resource_type.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace content {
class NavigationUIData;
}  // namespace content

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
      content::NavigationUIData* navigation_ui_data,
      content::WebContents::Getter web_contents_getter);

  ~URLLoaderThrottle() override;

  // blink::URLLoaderThrottle
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;
  void WillRedirectRequest(net::RedirectInfo* redirect_info,
                           const network::mojom::URLResponseHead& response_head,
                           bool* defer,
                           std::vector<std::string>* headers_to_remove,
                           net::HttpRequestHeaders* modified_headers) override;
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
  GURL request_referrer_;
  net::HttpRequestHeaders request_headers_;
  content::ResourceType request_resource_type_;

  base::OnceClosure destruction_callback_;

  DISALLOW_COPY_AND_ASSIGN(URLLoaderThrottle);
};

}  // namespace signin

#endif  // CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_URL_LOADER_THROTTLE_H_
