// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/subresource_redirect/subresource_redirect_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace subresource_redirect {

namespace {

// Naive function that escapes :, / characters in URL. Useful for simple tests.
std::string EscapeURLForQueryParam(std::string url) {
  base::ReplaceChars(url, ":", base::StringPrintf("%%%0X", ':'), &url);
  base::ReplaceChars(url, "/", base::StringPrintf("%%%0X", '/'), &url);
  return url;
}

}  // namespace

TEST(SubresourceRedirectUtilTest, GetRobotsServerURL) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{blink::features::kSubresourceRedirect,
        {{"enable_login_robots_based_compression", "true"},
         {"enable_public_image_hints_based_compression", "false"},
         {"enable_login_robots_for_low_memory", "true"}}}},
      {});

  for (auto* origin :
       {"https://foo.com/", "https://www.foo.com/", "http://foo.com/"}) {
    EXPECT_EQ(
        GURL("https://litepages.googlezip.net/robots?u=" +
             EscapeURLForQueryParam(base::StrCat({origin, "robots.txt"}))),
        GetRobotsServerURL(url::Origin::Create(GURL(origin))));
  }
}

TEST(SubresourceRedirectUtilTest, GetRobotsServerURL_ModifiedLitePagesOrigin) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{blink::features::kSubresourceRedirect,
        {{"enable_login_robots_based_compression", "true"},
         {"enable_public_image_hints_based_compression", "false"},
         {"lite_page_robots_origin", "https://modified.litepages.com/"},
         {"enable_login_robots_for_low_memory", "true"}}}},
      {});

  for (auto* origin :
       {"https://foo.com/", "https://www.foo.com/", "http://foo.com/"}) {
    EXPECT_EQ(
        GURL("https://modified.litepages.com/robots?u=" +
             EscapeURLForQueryParam(base::StrCat({origin, "robots.txt"}))),
        GetRobotsServerURL(url::Origin::Create(GURL(origin))));
  }
}

}  // namespace subresource_redirect
