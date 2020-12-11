// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/chrome_signin_url_loader_throttle.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/signin/chrome_signin_helper.h"
#include "chrome/browser/signin/header_modification_delegate.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace signin {

class URLLoaderThrottle::ThrottleRequestAdapter : public ChromeRequestAdapter {
 public:
  ThrottleRequestAdapter(URLLoaderThrottle* throttle,
                         const net::HttpRequestHeaders& original_headers,
                         net::HttpRequestHeaders* modified_headers,
                         std::vector<std::string>* headers_to_remove)
      : ChromeRequestAdapter(throttle->request_url_,
                             original_headers,
                             modified_headers,
                             headers_to_remove),
        throttle_(throttle) {}

  ~ThrottleRequestAdapter() override = default;

  // ChromeRequestAdapter
  content::WebContents::Getter GetWebContentsGetter() const override {
    return throttle_->web_contents_getter_;
  }

  blink::mojom::ResourceType GetResourceType() const override {
    return throttle_->request_resource_type_;
  }

  GURL GetReferrerOrigin() const override {
    return throttle_->request_referrer_.GetOrigin();
  }

  void SetDestructionCallback(base::OnceClosure closure) override {
    if (!throttle_->destruction_callback_)
      throttle_->destruction_callback_ = std::move(closure);
  }

 private:
  URLLoaderThrottle* const throttle_;

  DISALLOW_COPY_AND_ASSIGN(ThrottleRequestAdapter);
};

class URLLoaderThrottle::ThrottleResponseAdapter : public ResponseAdapter {
 public:
  ThrottleResponseAdapter(URLLoaderThrottle* throttle,
                          net::HttpResponseHeaders* headers)
      : throttle_(throttle), headers_(headers) {}

  ~ThrottleResponseAdapter() override = default;

  // ResponseAdapter
  content::WebContents::Getter GetWebContentsGetter() const override {
    return throttle_->web_contents_getter_;
  }

  bool IsMainFrame() const override {
    return throttle_->request_resource_type_ ==
           blink::mojom::ResourceType::kMainFrame;
  }

  GURL GetOrigin() const override {
    return throttle_->request_url_.GetOrigin();
  }

  const net::HttpResponseHeaders* GetHeaders() const override {
    return headers_;
  }

  void RemoveHeader(const std::string& name) override {
    headers_->RemoveHeader(name);
  }

  base::SupportsUserData::Data* GetUserData(const void* key) const override {
    return throttle_->GetUserData(key);
  }

  void SetUserData(
      const void* key,
      std::unique_ptr<base::SupportsUserData::Data> data) override {
    throttle_->SetUserData(key, std::move(data));
  }

 private:
  URLLoaderThrottle* const throttle_;
  net::HttpResponseHeaders* headers_;

  DISALLOW_COPY_AND_ASSIGN(ThrottleResponseAdapter);
};

// static
std::unique_ptr<URLLoaderThrottle> URLLoaderThrottle::MaybeCreate(
    std::unique_ptr<HeaderModificationDelegate> delegate,
    content::WebContents::Getter web_contents_getter) {
  if (!delegate->ShouldInterceptNavigation(web_contents_getter.Run()))
    return nullptr;

  return base::WrapUnique(new URLLoaderThrottle(
      std::move(delegate), std::move(web_contents_getter)));
}

URLLoaderThrottle::~URLLoaderThrottle() {
  if (destruction_callback_)
    std::move(destruction_callback_).Run();
}

void URLLoaderThrottle::WillStartRequest(network::ResourceRequest* request,
                                         bool* defer) {
  request_url_ = request->url;
  request_referrer_ = request->referrer;
  request_resource_type_ =
      static_cast<blink::mojom::ResourceType>(request->resource_type);

  net::HttpRequestHeaders modified_request_headers;
  std::vector<std::string> to_be_removed_request_headers;

  ThrottleRequestAdapter adapter(this, request->headers,
                                 &modified_request_headers,
                                 &to_be_removed_request_headers);
  delegate_->ProcessRequest(&adapter, GURL() /* redirect_url */);

  request->headers.MergeFrom(modified_request_headers);
  for (const std::string& name : to_be_removed_request_headers)
    request->headers.RemoveHeader(name);

  // We need to keep a full copy of the request headers for later calls to
  // FixAccountConsistencyRequestHeader. Perhaps this could be replaced with
  // more specific per-request state.
  request_headers_.CopyFrom(request->headers);
  request_cors_exempt_headers_.CopyFrom(request->cors_exempt_headers);
}

void URLLoaderThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& response_head,
    bool* /* defer */,
    std::vector<std::string>* to_be_removed_request_headers,
    net::HttpRequestHeaders* modified_request_headers,
    net::HttpRequestHeaders* modified_cors_exempt_request_headers) {
  ThrottleRequestAdapter request_adapter(this, request_headers_,
                                         modified_request_headers,
                                         to_be_removed_request_headers);
  delegate_->ProcessRequest(&request_adapter, redirect_info->new_url);

  request_headers_.MergeFrom(*modified_request_headers);
  for (const std::string& name : *to_be_removed_request_headers)
    request_headers_.RemoveHeader(name);

  // Modifications to |response_head.headers| will be passed to the
  // URLLoaderClient even though |response_head| is const.
  ThrottleResponseAdapter response_adapter(this, response_head.headers.get());
  delegate_->ProcessResponse(&response_adapter, redirect_info->new_url);

  request_url_ = redirect_info->new_url;
  request_referrer_ = GURL(redirect_info->new_referrer);
}

void URLLoaderThrottle::WillProcessResponse(
    const GURL& response_url,
    network::mojom::URLResponseHead* response_head,
    bool* defer) {
  ThrottleResponseAdapter adapter(this, response_head->headers.get());
  delegate_->ProcessResponse(&adapter, GURL() /* redirect_url */);
}

URLLoaderThrottle::URLLoaderThrottle(
    std::unique_ptr<HeaderModificationDelegate> delegate,
    content::WebContents::Getter web_contents_getter)
    : delegate_(std::move(delegate)),
      web_contents_getter_(std::move(web_contents_getter)) {}

}  // namespace signin
