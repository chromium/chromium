// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/network_service/aw_url_loader_throttle.h"

#include "android_webview/browser/aw_resource_context.h"
#include "net/http/http_response_headers.h"

namespace android_webview {

AwURLLoaderThrottle::AwURLLoaderThrottle(AwResourceContext* aw_resource_context)
    : aw_resource_context_(aw_resource_context) {}

AwURLLoaderThrottle::~AwURLLoaderThrottle() = default;

void AwURLLoaderThrottle::WillStartRequest(network::ResourceRequest* request,
                                           bool* defer) {
  AddExtraHeadersIfNeeded(request->url, &request->headers);
}

void AwURLLoaderThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& response_head,
    bool* defer,
    std::vector<std::string>* to_be_removed_request_headers,
    net::HttpRequestHeaders* modified_request_headers) {
  AddExtraHeadersIfNeeded(redirect_info->new_url, modified_request_headers);
}

void AwURLLoaderThrottle::AddExtraHeadersIfNeeded(
    const GURL& url,
    net::HttpRequestHeaders* headers) {
  std::string extra_headers = aw_resource_context_->GetExtraHeaders(url);
  if (extra_headers.empty())
    return;

  net::HttpRequestHeaders temp_headers;
  temp_headers.AddHeadersFromString(extra_headers);
  for (net::HttpRequestHeaders::Iterator it(temp_headers); it.GetNext();)
    headers->SetHeaderIfMissing(it.name(), it.value());
}

}  // namespace android_webview
