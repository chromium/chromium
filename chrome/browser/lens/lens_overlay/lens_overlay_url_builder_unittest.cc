// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lens/lens_overlay/lens_overlay_url_builder.h"

#include "base/base64url.h"
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

TEST_F(LensOverlayUrlBuilderTest, BuildTextOnlySearchURL) {
  std::string text_query = "Apples";
  std::string expected_url = base::StringPrintf(
      "%s?q=%s&gsc=1&masfc=c", kResultsSearchBaseUrl, text_query.c_str());

  EXPECT_EQ(lens::BuildTextOnlySearchURL(text_query), expected_url);
}

TEST_F(LensOverlayUrlBuilderTest, BuildTextOnlySearchURLEmpty) {
  std::string text_query = "";
  std::string expected_url =
      base::StringPrintf("%s?q=&gsc=1&masfc=c", kResultsSearchBaseUrl);

  EXPECT_EQ(lens::BuildTextOnlySearchURL(text_query), expected_url);
}

TEST_F(LensOverlayUrlBuilderTest, BuildTextOnlySearchURLPunctuation) {
  std::string text_query = "Red Apples!?#";
  std::string escaped_text_query =
      base::EscapeQueryParamValue(text_query, /*use_plus=*/true);
  std::string expected_url = base::StringPrintf(
      "%s?q=%s&gsc=1&masfc=c", kResultsSearchBaseUrl,
      escaped_text_query.c_str());

  EXPECT_EQ(lens::BuildTextOnlySearchURL(text_query), expected_url);
}

TEST_F(LensOverlayUrlBuilderTest, BuildTextOnlySearchURLWhitespace) {
  std::string text_query = "Red Apples";
  std::string escaped_text_query =
      base::EscapeQueryParamValue(text_query, /*use_plus=*/true);
  std::string expected_url = base::StringPrintf(
      "%s?q=%s&gsc=1&masfc=c", kResultsSearchBaseUrl,
      escaped_text_query.c_str());

  EXPECT_EQ(lens::BuildTextOnlySearchURL(text_query), expected_url);
}

TEST_F(LensOverlayUrlBuilderTest, BuildLensSearchURLEmptyClusterInfo) {
  std::string text_query = "Green Apples";
  std::string escaped_text_query =
      base::EscapeQueryParamValue(text_query, /*use_plus=*/true);
  uint64_t uuid = 12345;
  int sequence_id = 1;
  int image_sequence_id = 3;

  std::unique_ptr<lens::LensOverlayRequestId> request_id =
      std::make_unique<lens::LensOverlayRequestId>();
  lens::LensOverlayClusterInfo cluster_info;
  request_id->set_uuid(uuid);
  request_id->set_sequence_id(sequence_id);
  request_id->set_image_sequence_id(image_sequence_id);

  std::string serialized_request_id;
  CHECK(request_id.get()->SerializeToString(&serialized_request_id));
  std::string encoded_request_id;
  base::Base64UrlEncode(serialized_request_id,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_request_id);

  std::string expected_url =
      base::StringPrintf("%s?gsc=1&masfc=c&q=%s&gsessionid=&udm=24&vsrid=%s",
                         kResultsSearchBaseUrl, escaped_text_query.c_str(),
                         encoded_request_id.c_str());

  EXPECT_EQ(
      lens::BuildLensSearchURL(text_query, std::move(request_id), cluster_info),
      expected_url);
}

TEST_F(LensOverlayUrlBuilderTest, BuildLensSearchURLWithSessionId) {
  std::string text_query = "Green Apples";
  std::string escaped_text_query =
      base::EscapeQueryParamValue(text_query, /*use_plus=*/true);
  uint64_t uuid = 12345;
  int sequence_id = 1;
  int image_sequence_id = 3;
  std::string search_session_id = "search_session_id";

  std::unique_ptr<lens::LensOverlayRequestId> request_id =
      std::make_unique<lens::LensOverlayRequestId>();
  lens::LensOverlayClusterInfo cluster_info;
  cluster_info.set_search_session_id(search_session_id);
  request_id->set_uuid(uuid);
  request_id->set_sequence_id(sequence_id);
  request_id->set_image_sequence_id(image_sequence_id);

  std::string serialized_request_id;
  CHECK(request_id.get()->SerializeToString(&serialized_request_id));
  std::string encoded_request_id;
  base::Base64UrlEncode(serialized_request_id,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_request_id);

  std::string expected_url =
      base::StringPrintf("%s?gsc=1&masfc=c&q=%s&gsessionid=%s&udm=24&vsrid=%s",
                         kResultsSearchBaseUrl, escaped_text_query.c_str(),
                         search_session_id.c_str(), encoded_request_id.c_str());

  EXPECT_EQ(
      lens::BuildLensSearchURL(text_query, std::move(request_id), cluster_info),
      expected_url);
}

TEST_F(LensOverlayUrlBuilderTest, BuildLensSearchURLWithNoTextQuery) {
  uint64_t uuid = 12345;
  int sequence_id = 1;
  int image_sequence_id = 3;
  std::string search_session_id = "search_session_id";

  std::unique_ptr<lens::LensOverlayRequestId> request_id =
      std::make_unique<lens::LensOverlayRequestId>();
  lens::LensOverlayClusterInfo cluster_info;
  cluster_info.set_search_session_id(search_session_id);
  request_id->set_uuid(uuid);
  request_id->set_sequence_id(sequence_id);
  request_id->set_image_sequence_id(image_sequence_id);

  std::string serialized_request_id;
  CHECK(request_id.get()->SerializeToString(&serialized_request_id));
  std::string encoded_request_id;
  base::Base64UrlEncode(serialized_request_id,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_request_id);

  std::string expected_url =
      base::StringPrintf("%s?gsc=1&masfc=c&q=&gsessionid=%s&udm=26&vsrid=%s",
                         kResultsSearchBaseUrl, search_session_id.c_str(),
                         encoded_request_id.c_str());

  EXPECT_EQ(lens::BuildLensSearchURL(std::nullopt, std::move(request_id),
                                     cluster_info),
            expected_url);
}

}  // namespace lens
