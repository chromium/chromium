// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_origin_matched_header.h"

#include "base/memory/scoped_refptr.h"
#include "components/origin_matcher/origin_matcher.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace android_webview {

TEST(AwOriginMatchedHeaderTest, MatchesNameValue) {
  scoped_refptr<AwOriginMatchedHeader> header =
      base::MakeRefCounted<AwOriginMatchedHeader>(
          "X-Custom-Header", "SomeValue", origin_matcher::OriginMatcher());

  // Test case-insensitive name match and case-sensitive value match
  EXPECT_TRUE(header->MatchesNameValue("x-custom-header", "SomeValue"));
  EXPECT_FALSE(header->MatchesNameValue("x-custom-header", "somevalue"));
  EXPECT_FALSE(header->MatchesNameValue("x-custom-header", "OtherValue"));

  // Test name-only match
  EXPECT_TRUE(header->MatchesNameValue("x-custom-header", std::nullopt));
  EXPECT_FALSE(header->MatchesNameValue("non-existent-header", std::nullopt));
}

TEST(AwOriginMatchedHeaderTest, MergedWithMatcher) {
  origin_matcher::OriginMatcher initial_matcher;
  ASSERT_TRUE(initial_matcher.AddRuleFromString("https://*.test.com"));

  scoped_refptr<AwOriginMatchedHeader> header =
      base::MakeRefCounted<AwOriginMatchedHeader>(
          "X-Custom-Header", "SomeValue", std::move(initial_matcher));

  origin_matcher::OriginMatcher other_matcher;
  ASSERT_TRUE(other_matcher.AddRuleFromString("https://*.example.com"));
  // intentionally duplicate rule.
  ASSERT_TRUE(other_matcher.AddRuleFromString("https://*.test.com"));

  scoped_refptr<AwOriginMatchedHeader> merged_header =
      header->MergedWithMatcher(std::move(other_matcher));

  // The name and value should be the same.
  EXPECT_EQ(header->name(), merged_header->name());
  EXPECT_EQ(header->value(), merged_header->value());

  // Check the origins.
  const url::Origin test_origin =
      url::Origin::Create(GURL("https://www.test.com"));
  const url::Origin example_origin =
      url::Origin::Create(GURL("https://www.example.com"));
  const url::Origin test_org_origin =
      url::Origin::Create(GURL("https://www.test.org"));

  EXPECT_TRUE(merged_header->MatchesOrigin(test_origin));
  EXPECT_TRUE(merged_header->MatchesOrigin(example_origin));
  EXPECT_FALSE(merged_header->MatchesOrigin(test_org_origin));
}

TEST(AwOriginMatchedHeaderTest, GetCombinedMatchingHeaders) {
  origin_matcher::OriginMatcher test_matcher;
  ASSERT_TRUE(test_matcher.AddRuleFromString("https://*.test.com"));

  origin_matcher::OriginMatcher example_matcher;
  ASSERT_TRUE(example_matcher.AddRuleFromString("https://*.example.com"));

  std::vector<scoped_refptr<AwOriginMatchedHeader>> headers;
  headers.push_back(base::MakeRefCounted<AwOriginMatchedHeader>(
      "X-Custom-Header", "Value1", test_matcher));
  // Ensure that headers with different capitalization are coalesced.
  headers.push_back(base::MakeRefCounted<AwOriginMatchedHeader>(
      "x-cUSTOM-hEADER", "Value2", test_matcher));

  headers.push_back(base::MakeRefCounted<AwOriginMatchedHeader>(
      "Another-Header", "AnotherValue", example_matcher));

  // Test with an origin that matches multiple headers with the same name.
  const url::Origin test_origin =
      url::Origin::Create(GURL("https://www.test.com"));
  std::vector<std::pair<std::string_view, std::string>> combined_headers =
      AwOriginMatchedHeader::GetCombinedMatchingHeaders(headers, test_origin);

  ASSERT_EQ(1u, combined_headers.size());
  const auto& [name, value] = combined_headers[0];
  EXPECT_EQ("X-Custom-Header", name);
  EXPECT_EQ("Value1,Value2", value);

  // Test with an origin that matches a single header.
  const url::Origin example_origin =
      url::Origin::Create(GURL("https://www.example.com"));
  combined_headers = AwOriginMatchedHeader::GetCombinedMatchingHeaders(
      headers, example_origin);

  ASSERT_EQ(1u, combined_headers.size());
  const auto& [name2, value2] = combined_headers[0];
  EXPECT_EQ("Another-Header", name2);
  EXPECT_EQ("AnotherValue", value2);

  // Test with an origin that doesn't match any headers.
  const url::Origin test_org_origin =
      url::Origin::Create(GURL("https://www.test.org"));
  combined_headers = AwOriginMatchedHeader::GetCombinedMatchingHeaders(
      headers, test_org_origin);
  EXPECT_TRUE(combined_headers.empty());
}

}  // namespace android_webview
