// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/page_context_util.h"

#include <string_view>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/actor/core/safety_list_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ttc {
namespace {

class PageContextUtilTest : public testing::Test {
 protected:
  void TearDown() override {
    // SafetyListManager is a process-wide singleton; clear any lists a test
    // set.
    actor::SetSafetyListsForTesting(actor::SafetyListManager::GetInstance(),
                                    "{}");
  }

  bool IsUrlSupported(const GURL& url) {
    base::test::TestFuture<bool> supported;
    IsUrlSupportedForPageContext(url, supported.GetCallback());
    return supported.Get();
  }

 private:
  base::test::TaskEnvironment task_environment_;
};

// HTTP(S) pages and the New Tab Page are supported.
class SupportedUrlTest : public PageContextUtilTest,
                         public testing::WithParamInterface<std::string_view> {
};

TEST_P(SupportedUrlTest, IsSupported) {
  EXPECT_TRUE(IsUrlSupported(GURL(GetParam())));
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
class UnsupportedUrlTest
    : public PageContextUtilTest,
      public testing::WithParamInterface<std::string_view> {};

TEST_P(UnsupportedUrlTest, IsNotSupported) {
  EXPECT_FALSE(IsUrlSupported(GURL(GetParam())));
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

// HTTP(S) URLs for which the actor safety lists block a navigation from the URL
// to itself are unsupported.
TEST_F(PageContextUtilTest, SafetyLists) {
  actor::SetSafetyListsForTesting(actor::SafetyListManager::GetInstance(),
                                  R"json(
    {
      "navigation_allowed": [
        { "from": "*", "to": "[*.]allowed.blocked.com" }
      ],
      "navigation_blocked": [
        { "from": "*", "to": "[*.]blocked.com" },
        { "from": "[*.]other.com", "to": "[*.]cross-origin-only.com" }
      ]
    }
  )json");

  EXPECT_FALSE(IsUrlSupported(GURL("https://blocked.com/path")));
  EXPECT_TRUE(IsUrlSupported(GURL("https://allowed.blocked.com/path")));
  EXPECT_TRUE(IsUrlSupported(GURL("https://cross-origin-only.com/path")));
  EXPECT_TRUE(IsUrlSupported(GURL("https://unlisted.com/path")));
}

}  // namespace
}  // namespace ttc
