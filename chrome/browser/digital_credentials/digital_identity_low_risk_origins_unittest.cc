// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/digital_credentials/digital_identity_low_risk_origins.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace digital_credentials {

url::Origin CreateOrigin(const std::string& spec) {
  return url::Origin::Create(GURL(spec));
}

// Tests for the original IsLowRiskOrigin function (uses the production list)
TEST(IsLowRiskOriginTest, ProductionListIsEmptySoAlwaysFalse) {
  // kKnownLowRiskOrigins in the .cc file is empty.
  EXPECT_FALSE(IsLowRiskOrigin(CreateOrigin("https://example.com")));
  EXPECT_FALSE(
      IsLowRiskOrigin(CreateOrigin("https://www.digital-credentials.dev")));
}

// Tests for IsLowRiskOriginMatcherForTesting (allows custom lists)

TEST(IsLowRiskOriginMatcherTest, ExactMatch) {
  std::vector<std::string> test_origins = {"https://example.com"};
  EXPECT_TRUE(IsLowRiskOriginMatcherForTesting(
      CreateOrigin("https://example.com"), test_origins));
}

TEST(IsLowRiskOriginMatcherTest, WwwMatchesNonWwwInList) {
  std::vector<std::string> test_origins = {
      "https://example.com"};  // Non-www in list
  EXPECT_TRUE(
      IsLowRiskOriginMatcherForTesting(CreateOrigin("https://www.example.com"),
                                       test_origins));  // Check www
}

TEST(IsLowRiskOriginMatcherTest, NonWwwMatchesWwwInList) {
  std::vector<std::string> test_origins = {
      "https://www.example.com"};  // Www in list
  EXPECT_TRUE(
      IsLowRiskOriginMatcherForTesting(CreateOrigin("https://example.com"),
                                       test_origins));  // Check non-www
}

TEST(IsLowRiskOriginMatcherTest, NoMatchDifferentDomain) {
  std::vector<std::string> test_origins = {"https://example.com"};
  EXPECT_FALSE(IsLowRiskOriginMatcherForTesting(
      CreateOrigin("https://different.com"), test_origins));
}

TEST(IsLowRiskOriginMatcherTest, SchemeMismatch) {
  std::vector<std::string> test_origins = {"https://example.com"};
  EXPECT_FALSE(IsLowRiskOriginMatcherForTesting(
      CreateOrigin("http://example.com"), test_origins));
  EXPECT_FALSE(IsLowRiskOriginMatcherForTesting(
      CreateOrigin("http://www.example.com"), test_origins));
}

TEST(IsLowRiskOriginMatcherTest, PortMismatch) {
  std::vector<std::string> test_origins = {
      "https://example.com"};  // Default port 443
  EXPECT_FALSE(IsLowRiskOriginMatcherForTesting(
      CreateOrigin("https://example.com:8080"), test_origins));
}

TEST(IsLowRiskOriginMatcherTest, SubdomainMismatch) {
  std::vector<std::string> test_origins = {"https://example.com"};
  // "www" is handled, other subdomains are not.
  EXPECT_FALSE(IsLowRiskOriginMatcherForTesting(
      CreateOrigin("https://sub.example.com"), test_origins));
}

TEST(IsLowRiskOriginMatcherTest, PathIsIgnored) {
  std::vector<std::string> test_origins = {"https://example.com"};
  EXPECT_TRUE(IsLowRiskOriginMatcherForTesting(
      CreateOrigin("https://example.com/some/path"), test_origins));
}

TEST(IsLowRiskOriginMatcherTest, EmptyList) {
  std::vector<std::string> empty_origins_list = {};
  EXPECT_FALSE(IsLowRiskOriginMatcherForTesting(
      CreateOrigin("https://example.com"), empty_origins_list));
}

TEST(IsLowRiskOriginMatcherTest, MultipleEntriesInList_Match) {
  std::vector<std::string> test_origins = {"https://another.com",
                                           "https://example.com"};
  EXPECT_TRUE(IsLowRiskOriginMatcherForTesting(
      CreateOrigin("https://example.com"), test_origins));
}

TEST(IsLowRiskOriginMatcherTest, MultipleEntriesInList_NoMatch) {
  std::vector<std::string> test_origins = {"https://another.com",
                                           "https://yetanother.com"};
  EXPECT_FALSE(IsLowRiskOriginMatcherForTesting(
      CreateOrigin("https://example.com"), test_origins));
}

}  // namespace digital_credentials
