// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/request_matcher/aw_request_matcher.h"

#include <string>

#include "base/test/gmock_expected_support.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace android_webview {

class AwRequestMatcherTest : public testing::Test {
 public:
  AwRequestMatcherTest() = default;
  ~AwRequestMatcherTest() override = default;

 protected:
  network::ResourceRequest CreateTestRequest(std::string_view url,
                                             std::string_view method) {
    network::ResourceRequest request;
    request.url = GURL(url);
    request.method = method;
    return request;
  }

  network::ResourceRequest CreateMainFrameDocumentRequest(
      std::string_view url = "https://example.com",
      std::string_view method = "GET") {
    auto result = CreateTestRequest(url, method);
    result.destination = network::mojom::RequestDestination::kDocument;
    result.mode = network::mojom::RequestMode::kNavigate;
    return result;
  }

  network::ResourceRequest CreateSubFrameDocumentRequest(
      std::string_view url = "https://example.com/subframe",
      std::string_view method = "GET") {
    auto result = CreateTestRequest(url, method);
    result.destination = network::mojom::RequestDestination::kIframe;
    result.mode = network::mojom::RequestMode::kNavigate;
    return result;
  }

  network::ResourceRequest CreateSubResourceRequest(
      std::string_view url = "https://example.com/my-image.png",
      std::string_view method = "GET") {
    auto result = CreateTestRequest(url, method);
    result.destination = network::mojom::RequestDestination::kImage;
    result.mode = network::mojom::RequestMode::kSameOrigin;
    return result;
  }
};

TEST_F(AwRequestMatcherTest, SingleUrlPattern) {
  ASSERT_OK_AND_ASSIGN(auto matcher,
                       AwRequestMatcher::Create(/*url_pattern_strings=*/{
                           "https://www.example.com/foo"}));

  EXPECT_TRUE(matcher->Matches(
      CreateMainFrameDocumentRequest("https://www.example.com/foo")))
      << "Same URL request should be matched";
  EXPECT_TRUE(matcher->Matches(
      CreateSubFrameDocumentRequest("https://www.example.com/foo")))
      << "Same URL request should be matched for subframes";
  EXPECT_TRUE(
      matcher->Matches(CreateSubResourceRequest("https://www.example.com/foo")))
      << "Same URL request should be matched for subresources";
  EXPECT_FALSE(matcher->Matches(
      CreateMainFrameDocumentRequest("https://www.example.com/foobar")))
      << "Different path should not be matched";
  EXPECT_FALSE(matcher->Matches(
      CreateMainFrameDocumentRequest("https://www.example.com/bar/foo")))
      << "Suffix path should not be matched";
  EXPECT_FALSE(matcher->Matches(
      CreateMainFrameDocumentRequest("https://www.anotherexample.com/foo")))
      << "Different host should not be matched";
  EXPECT_FALSE(matcher->Matches(
      CreateMainFrameDocumentRequest("https://www.example.com/foo/bar")))
      << "Subpath should not be matched";
  EXPECT_TRUE(matcher->Matches(
      CreateMainFrameDocumentRequest("https://www.example.com/foo", "POST")))
      << "Same URL request with different method should be matched";
}

TEST_F(AwRequestMatcherTest, MultipleUrlPatterns_MatchesAny) {
  ASSERT_OK_AND_ASSIGN(
      auto matcher, AwRequestMatcher::Create(/*url_pattern_strings=*/
                                             {"https://www.example.com/foo",
                                              "https://www.example.com/bar",
                                              "https://anotherexample.com"}));

  EXPECT_TRUE(matcher->Matches(
      CreateMainFrameDocumentRequest("https://www.example.com/foo")))
      << "First exact URL request should be matched";
  EXPECT_TRUE(matcher->Matches(
      CreateMainFrameDocumentRequest("https://www.example.com/bar")))
      << "Second exact URL request should be matched";
  EXPECT_TRUE(matcher->Matches(
      CreateMainFrameDocumentRequest("https://anotherexample.com")))
      << "Third exact URL request should be matched";
  EXPECT_TRUE(matcher->Matches(
      CreateMainFrameDocumentRequest("https://anotherexample.com/foo/bar")))
      << "Subpath of pathless URL pattern should be matched";
  EXPECT_FALSE(matcher->Matches(
      CreateMainFrameDocumentRequest("https://www.noneoftheexamples.com")))
      << "Different URL should not be matched";
  EXPECT_TRUE(matcher->Matches(
      CreateSubFrameDocumentRequest("https://www.example.com/bar")))
      << "Second exact URL request should be matched for subframes";
  EXPECT_TRUE(
      matcher->Matches(CreateSubResourceRequest("https://anotherexample.com")))
      << "Third exact URL request should be matched for subresources";
}

TEST_F(AwRequestMatcherTest, Create_NoUrlPatterns_FailsWithError) {
  auto create_result = AwRequestMatcher::Create(/*url_pattern_strings=*/{});

  EXPECT_FALSE(create_result.has_value());
  EXPECT_EQ(create_result.error(),
            "Must provide at least one URL pattern string.");
}

TEST_F(AwRequestMatcherTest,
       CreateFromConfig_InvalidUrlPatterns_FailsWithError) {
  auto create_result =
      AwRequestMatcher::Create(/*url_pattern_strings=*/{"/no-protocol"});

  EXPECT_FALSE(create_result.has_value());
  EXPECT_TRUE(
      create_result.error().find(
          "Failed to parse URL pattern at index 0 (\"/no-protocol\"):") !=
      std::string::npos);
}

}  // namespace android_webview
