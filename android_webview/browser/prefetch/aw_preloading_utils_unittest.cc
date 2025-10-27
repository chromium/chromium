// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/prefetch/aw_preloading_utils.h"

#include "net/http/http_request_headers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {

using AwPreloadingUtilsTest = testing::Test;

TEST_F(AwPreloadingUtilsTest, GetShouldBypassHttpCacheFromHeaders) {
  // Header value is "1" and should be removed.
  {
    net::HttpRequestHeaders headers;
    headers.SetHeader(kDisableHttpCacheHeader, "1");
    EXPECT_TRUE(
        GetShouldBypassHttpCacheFromHeaders(headers, /*remove_header=*/true));
    EXPECT_FALSE(headers.HasHeader(kDisableHttpCacheHeader));
  }

  // Header value is "1" and should be kept.
  {
    net::HttpRequestHeaders headers;
    headers.SetHeader(kDisableHttpCacheHeader, "1");
    EXPECT_TRUE(
        GetShouldBypassHttpCacheFromHeaders(headers, /*remove_header=*/false));
    EXPECT_TRUE(headers.HasHeader(kDisableHttpCacheHeader));
  }

  // Header value is "0", should return false and remove the header.
  {
    net::HttpRequestHeaders headers;
    headers.SetHeader(kDisableHttpCacheHeader, "0");
    EXPECT_FALSE(
        GetShouldBypassHttpCacheFromHeaders(headers, /*remove_header=*/true));
    EXPECT_FALSE(headers.HasHeader(kDisableHttpCacheHeader));
  }

  // Header is not present.
  {
    net::HttpRequestHeaders headers;
    EXPECT_FALSE(
        GetShouldBypassHttpCacheFromHeaders(headers, /*remove_header=*/true));
  }

  // Header has a different value, should return false and remove the header.
  {
    net::HttpRequestHeaders headers;
    headers.SetHeader(kDisableHttpCacheHeader, "true");
    EXPECT_FALSE(
        GetShouldBypassHttpCacheFromHeaders(headers, /*remove_header=*/true));
    EXPECT_FALSE(headers.HasHeader(kDisableHttpCacheHeader));
  }

  // Header name is case-insensitive.
  {
    net::HttpRequestHeaders headers;
    headers.SetHeader("x-disable-http-cache", "1");
    EXPECT_TRUE(
        GetShouldBypassHttpCacheFromHeaders(headers, /*remove_header=*/true));
    EXPECT_FALSE(headers.HasHeader("x-disable-http-cache"));
    EXPECT_FALSE(headers.HasHeader(kDisableHttpCacheHeader));
  }
}

}  // namespace android_webview
