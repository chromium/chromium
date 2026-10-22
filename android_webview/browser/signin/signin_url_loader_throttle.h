// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_SIGNIN_SIGNIN_URL_LOADER_THROTTLE_H_
#define ANDROID_WEBVIEW_BROWSER_SIGNIN_SIGNIN_URL_LOADER_THROTTLE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "components/signin/core/browser/cookie_settings_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_request_headers.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace net {
class HttpResponseHeaders;
}  // namespace net

namespace network {
struct HttpRequestHeadersUpdateParams;
}  // namespace network

namespace android_webview {

class AwGoogleAccountsDelegate;

// ULRLoaderThrottle that allows WebView to handle the sign-in protocol from
// the signin component if a callback is set by the embedding application.
//
// The sign-in mirror protocol works by attaching the `X-Chrome-Connected`
// header to requests sent to Google servers that handle login state. When this
// header is present, the server will not make any account changes by updating
// cookies, but will instead respond to user journeys that initiate account
// changes with a 204 response and a `X-Chrome-Manage-Accounts` header with
// further information for the embedder.
//
// This throttle will attach the request header on all relevant requests, and
// will forward the response header information in an async callback to the
// embedding application.
//
// See `components/signin/core/browser/signin_header_helper.cc` for the Chrome
// implementation of this protocol.
class SigninUrlLoaderThrottle : public blink::URLLoaderThrottle {
 public:
  using GoogleAccountsDelegateGetter =
      base::RepeatingCallback<AwGoogleAccountsDelegate*()>;

  ~SigninUrlLoaderThrottle() override;

  static std::unique_ptr<SigninUrlLoaderThrottle> Create(
      content::BrowserContext* browser_context,
      content::WebContents::Getter web_contents_getter);

  static std::unique_ptr<SigninUrlLoaderThrottle> CreateForTesting(
      content::BrowserContext* browser_context,
      signin::CookieSettingsCallback cookie_settings_callback,
      GoogleAccountsDelegateGetter delegate_getter);

  // blink::URLLoaderThrottle:
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;
  void WillRedirectRequest(
      net::RedirectInfo* redirect_info,
      const network::mojom::URLResponseHead& response_head,
      bool* defer,
      network::HttpRequestHeadersUpdateParams* headers_update_params) override;
  void WillProcessResponse(const GURL& response_url,
                           network::mojom::URLResponseHead* response_head,
                           bool* defer) override;

 private:
  SigninUrlLoaderThrottle(
      content::BrowserContext* browser_context,
      signin::CookieSettingsCallback cookie_settings_callback,
      GoogleAccountsDelegateGetter delegate_getter);

  void ProcessRequest(const GURL& new_url,
                      net::HttpRequestHeaders* original_headers,
                      std::vector<std::string>* headers_to_remove,
                      net::HttpRequestHeaders* modified_headers);
  void ProcessResponse(const net::HttpResponseHeaders* headers);

  raw_ptr<content::BrowserContext> browser_context_;
  signin::CookieSettingsCallback cookie_settings_callback_;
  GoogleAccountsDelegateGetter delegate_getter_;
  net::HttpRequestHeaders request_headers_;
  // Tracks the last request URL in use, so ProcessResponse can reference it.
  GURL request_url_;
  bool is_navigation_request_ = false;
  bool is_outermost_main_frame_ = false;
  bool response_header_processed_ = false;
};
}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_SIGNIN_SIGNIN_URL_LOADER_THROTTLE_H_
