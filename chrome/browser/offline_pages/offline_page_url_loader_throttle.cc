// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/offline_page_url_loader_throttle.h"

#include "components/offline_pages/core/request_header/offline_page_header.h"
#include "services/network/public/cpp/http_request_headers_update_params.h"
#include "services/network/public/cpp/resource_request.h"

namespace offline_pages {

OfflinePageURLLoaderThrottle::OfflinePageURLLoaderThrottle() = default;

OfflinePageURLLoaderThrottle::~OfflinePageURLLoaderThrottle() = default;

void OfflinePageURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  request->headers.RemoveHeader(kOfflinePageHeader);
  request->cors_exempt_headers.RemoveHeader(kOfflinePageHeader);
}

void OfflinePageURLLoaderThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& response_head,
    bool* defer,
    network::HttpRequestHeadersUpdateParams* headers_update_params) {
  headers_update_params->RemoveHeader(kOfflinePageHeader);
}

}  // namespace offline_pages
