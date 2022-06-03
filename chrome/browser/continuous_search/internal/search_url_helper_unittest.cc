// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/continuous_search/internal/search_url_helper.h"

#include <string>

#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace continuous_search {

namespace {

constexpr char kSrpUrl[] = "https://www.google.com/search";

}  // namespace

TEST(SearchUrlHelper, ExtractSrpUrlWithEscape) {
  auto result = ExtractSearchQueryIfValidUrl(
      GURL(base::StrCat({kSrpUrl, R"(?q=foo%5Ebar%25baz)"})));
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), R"(foo^bar%baz)");
}

TEST(SearchUrlHelper, ExtractSrpUrlWithSpace) {
  auto result =
      ExtractSearchQueryIfValidUrl(GURL(base::StrCat({kSrpUrl, "?q=cat+dog"})));
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), "cat dog");
}

TEST(SearchUrlHelper, ExtractSrpUrlNewsTab) {
  auto result = ExtractSearchQueryIfValidUrl(
      GURL(base::StrCat({kSrpUrl, "?q=foo&tbm=nws"})));
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), "foo");
}

TEST(SearchUrlHelper, ExtractSrpUrlNoQuery) {
  EXPECT_FALSE(
      ExtractSearchQueryIfValidUrl(GURL(base::StrCat({kSrpUrl, "?foo=bar"})))
          .has_value());
}

TEST(SearchUrlHelper, NoExtractOtherUrl) {
  EXPECT_FALSE(ExtractSearchQueryIfValidUrl(GURL("https://www.example.com/"))
                   .has_value());
}

TEST(SearchUrlHelper, SrpPageCategory) {
  EXPECT_EQ(PageCategory::kOrganicSrp,
            GetSrpPageCategoryForUrl(GURL(base::StrCat({kSrpUrl, "?q=test"}))));
  EXPECT_EQ(PageCategory::kNewsSrp, GetSrpPageCategoryForUrl(GURL(base::StrCat(
                                        {kSrpUrl, "?q=test&tbm=nws"}))));
  EXPECT_EQ(PageCategory::kNone, GetSrpPageCategoryForUrl(GURL(base::StrCat(
                                     {kSrpUrl, "?q=test&tbm=invalid"}))));
}

class SearchUrlHelperRenderViewHostTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ~SearchUrlHelperRenderViewHostTest() override = default;

  void SetUp() override { content::RenderViewHostTestHarness::SetUp(); }
};

TEST_F(SearchUrlHelperRenderViewHostTest, NoOriginalUrlFromWebContents) {
  GURL url("https://www.committed-url.com/");

  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url);

  EXPECT_EQ(GURL(), GetOriginalUrlFromWebContents(web_contents()));
}

TEST_F(SearchUrlHelperRenderViewHostTest, OriginalUrlFromWebContents) {
  GURL original_url("https://www.original-url.com/");
  GURL url("https://www.committed-url.com/");

  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url);
  controller().GetLastCommittedEntry()->SetRedirectChain(
      std::vector<GURL>({original_url, url}));

  EXPECT_EQ(original_url, GetOriginalUrlFromWebContents(web_contents()));
}

}  // namespace continuous_search
