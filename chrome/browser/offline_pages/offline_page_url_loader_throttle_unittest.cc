// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/offline_page_url_loader_throttle.h"

#include "components/offline_pages/core/request_header/offline_page_header.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/http_request_headers_update_params.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace offline_pages {
namespace {

TEST(OfflinePageURLLoaderThrottleTest, WillStartRequestStripsOfflineHeader) {
  network::ResourceRequest request;
  request.url = GURL("https://example.com/page");
  request.headers.SetHeader(kOfflinePageHeader, "persist=1 reason=error id=7");
  request.cors_exempt_headers.SetHeader(kOfflinePageHeader,
                                        "persist=1 reason=error id=7");
  request.headers.SetHeader(net::HttpRequestHeaders::kAcceptLanguage, "en-US");

  OfflinePageURLLoaderThrottle throttle;
  bool defer = false;
  throttle.WillStartRequest(&request, &defer);

  EXPECT_FALSE(defer);
  EXPECT_FALSE(request.headers.HasHeader(kOfflinePageHeader));
  EXPECT_FALSE(request.cors_exempt_headers.HasHeader(kOfflinePageHeader));
  EXPECT_EQ("en-US", request.headers.GetHeader(
                         net::HttpRequestHeaders::kAcceptLanguage));
}

TEST(OfflinePageURLLoaderThrottleTest,
     WillRedirectRequestRemovesOfflineHeader) {
  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL("https://other.example/dest");
  auto response_head = network::mojom::URLResponseHead::New();

  net::HttpRequestHeaders headers;
  headers.SetHeader(kOfflinePageHeader, "persist=1 reason=error id=7");
  headers.SetHeader(net::HttpRequestHeaders::kAcceptLanguage, "en-US");
  net::HttpRequestHeaders cors_exempt_headers;
  cors_exempt_headers.SetHeader(kOfflinePageHeader,
                                "persist=1 reason=error id=7");

  network::HttpRequestHeadersUpdateParams update_params;
  // Also verify RemoveHeader clears any prior modified_headers re-addition.
  update_params.SetHeader(kOfflinePageHeader, "persist=1 reason=error id=7");

  OfflinePageURLLoaderThrottle throttle;
  bool defer = false;
  throttle.WillRedirectRequest(&redirect_info, *response_head, &defer,
                               &update_params);
  update_params.Apply(headers, cors_exempt_headers);

  EXPECT_FALSE(defer);
  EXPECT_FALSE(headers.HasHeader(kOfflinePageHeader));
  EXPECT_FALSE(cors_exempt_headers.HasHeader(kOfflinePageHeader));
  EXPECT_EQ("en-US",
            headers.GetHeader(net::HttpRequestHeaders::kAcceptLanguage));
}

}  // namespace
}  // namespace offline_pages
