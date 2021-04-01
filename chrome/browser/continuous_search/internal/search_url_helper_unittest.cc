// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/continuous_search/internal/search_url_helper.h"

#include <string>

#include "base/optional.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace continuous_search {

namespace {

constexpr char kSrpUrl[] = "https://www.google.com/search";

}

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

}  // namespace continuous_search
