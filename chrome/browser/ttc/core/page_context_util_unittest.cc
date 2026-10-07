// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/page_context_util.h"

#include <string_view>

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ttc {
namespace {

// HTTP(S) pages and the New Tab Page are supported.
class SupportedUrlTest : public testing::TestWithParam<std::string_view> {};

TEST_P(SupportedUrlTest, IsSupported) {
  EXPECT_TRUE(IsUrlSupportedForPageContext(GURL(GetParam())));
}

INSTANTIATE_TEST_SUITE_P(All,
                         SupportedUrlTest,
                         testing::Values("http://example.com/",
                                         "https://example.com/path?q=1#ref",
                                         "http://127.0.0.1:8080/",
                                         "chrome://newtab/",
                                         "chrome://new-tab-page/",
                                         "chrome://new-tab-page-third-party/"));

// All other schemes, including non-NTP chrome:// pages, are unsupported.
class UnsupportedUrlTest : public testing::TestWithParam<std::string_view> {};

TEST_P(UnsupportedUrlTest, IsNotSupported) {
  EXPECT_FALSE(IsUrlSupportedForPageContext(GURL(GetParam())));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    UnsupportedUrlTest,
    testing::Values(
        "chrome://settings/",
        "chrome://version/",
        "chrome://newtab-not-really/",
        "chrome-untrusted://new-tab-page/",
        "file:///tmp/index.html",
        "javascript:alert(1)",
        "about:blank",
        "data:text/html,hello",
        "blob:https://example.com/0b1c2d3e-4f50-6172-8394-a5b6c7d8e9f0",
        "chrome-extension://abcdefghijklmnopabcdefghijklmnop/page.html",
        "ftp://example.com/",
        "",
        "not a url"));

}  // namespace
}  // namespace ttc
