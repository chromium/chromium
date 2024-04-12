// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lens/lens_overlay/lens_overlay_url_builder.h"

#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "components/lens/lens_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace lens {

constexpr char kResultsSearchBaseUrl[] = "https://www.google.com/search";

class LensOverlayUrlBuilderTest : public testing::Test {
 public:
  void SetUp() override {
    // Set all the feature params here to keep the test consistent if future
    // default values are changed.
    feature_list_.InitAndEnableFeatureWithParameters(
        lens::features::kLensOverlay,
        {
            {"results-search-url", kResultsSearchBaseUrl},
        });
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(LensOverlayUrlBuilderTest, BuildSearchUrl) {
  std::string text_query = "Apples";
  std::string expected_url = base::StringPrintf(
      "%s?q=%s&gsc=1&masfc=c", kResultsSearchBaseUrl, text_query.c_str());

  EXPECT_EQ(lens::BuildSearchURL(text_query), expected_url);
}

TEST_F(LensOverlayUrlBuilderTest, BuildSearchUrlEmpty) {
  std::string text_query = "";
  std::string expected_url =
      base::StringPrintf("%s?q=&gsc=1&masfc=c", kResultsSearchBaseUrl);

  EXPECT_EQ(lens::BuildSearchURL(text_query), expected_url);
}

TEST_F(LensOverlayUrlBuilderTest, BuildSearchUrlPunctuation) {
  std::string text_query = "Red Apples!?#";
  std::string escaped_text_query =
      base::EscapeQueryParamValue(text_query, /*use_plus=*/true);
  std::string expected_url = base::StringPrintf(
      "%s?q=%s&gsc=1&masfc=c", kResultsSearchBaseUrl,
      escaped_text_query.c_str());

  EXPECT_EQ(lens::BuildSearchURL(text_query), expected_url);
}

TEST_F(LensOverlayUrlBuilderTest, BuildSearchUrlWhitespace) {
  std::string text_query = "Red Apples";
  std::string escaped_text_query =
      base::EscapeQueryParamValue(text_query, /*use_plus=*/true);
  std::string expected_url = base::StringPrintf(
      "%s?q=%s&gsc=1&masfc=c", kResultsSearchBaseUrl,
      escaped_text_query.c_str());

  EXPECT_EQ(lens::BuildSearchURL(text_query), expected_url);
}

}  // namespace lens
