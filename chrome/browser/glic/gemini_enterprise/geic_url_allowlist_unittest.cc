// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/gemini_enterprise/geic_url_allowlist.h"

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/public/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace geic {
namespace {

class GeicUrlAllowlistTest : public testing::Test {};

TEST_F(GeicUrlAllowlistTest, AllowedHosts) {
  // Representative production hosts.
  EXPECT_TRUE(IsAllowedGeminiEnterpriseHost(
      GURL("https://business.gemini.google/home")));
  EXPECT_TRUE(IsAllowedGeminiEnterpriseHost(
      GURL("https://vertexaisearch.cloud.google.com/home")));
  // Gemini Enterprise for Googlers.
  EXPECT_TRUE(IsAllowedGeminiEnterpriseHost(
      GURL("https://ucs-widget.corp.google.com/home")));
  // It intentionally also matches the pre-production `ucs-` pattern, so it can
  // be set from policy, Finch or the command line.
  EXPECT_TRUE(IsAllowedGeminiEnterprisePreProductionHost(
      GURL("https://ucs-widget.corp.google.com/home")));
}

TEST_F(GeicUrlAllowlistTest, PreProductionHostsAreNotPolicyHosts) {
  EXPECT_FALSE(IsAllowedGeminiEnterpriseHost(
      GURL("https://business-staging.gemini.google/home")));
  EXPECT_FALSE(IsAllowedGeminiEnterpriseHost(
      GURL("https://ucs-widget-staging.corp.google.com/home")));
  EXPECT_FALSE(IsAllowedGeminiEnterpriseHost(
      GURL("https://ucs-app-autopush.corp.google.com/home")));
  EXPECT_FALSE(IsAllowedGeminiEnterpriseHost(
      GURL("https://ucs-widget.corp.cloud.google/home")));
}

TEST_F(GeicUrlAllowlistTest, PreProductionHosts) {
  const char* const kAllowed[] = {
      "https://business-staging.gemini.google/home",
      "https://enterprise-autopush.gemini.google.com/home",
      "https://vertexaisearch-staging.gemini-us.google/home",
      "https://ucs-widget-autopush.corp.google.com/home",
      "https://ucs-app-autopush.corp.cloud.google/home",
      // A new environment label needs no code change.
      "https://business-preprod.gemini.google/home",
  };
  for (const char* url : kAllowed) {
    EXPECT_TRUE(IsAllowedGeminiEnterprisePreProductionHost(GURL(url))) << url;
  }
}

TEST_F(GeicUrlAllowlistTest, DisallowedPreProductionHosts) {
  const char* const kDisallowed[] = {
      // Production hosts are not pre-production hosts.
      "https://business.gemini.google/home",
      "https://enterprise.gemini.google.com/home",
      // Other products on the same parent domains.
      "https://gemini.google.com/home",
      "https://foo.gemini.google.com/home",
      "https://foo.corp.google.com/home",
      // Prefix without an environment label.
      "https://business-.gemini.google/home",
      "https://ucs-.corp.google.com/home",
      // Nested labels.
      "https://a.business-staging.gemini.google/home",
      "https://business-a.b.gemini.google/home",
      // Product prefix on the wrong parent domain.
      "https://business-staging.gemini.google.com/home",
      "https://ucs-widget.google.com/home",
      // Lookalike suffixes.
      "https://business-staging.gemini.google.evil.com/home",
      "https://ucs-widget.corp.google.com.evil.com/home",
      "https://localhost/home",
  };
  for (const char* url : kDisallowed) {
    EXPECT_FALSE(IsAllowedGeminiEnterprisePreProductionHost(GURL(url))) << url;
  }
}

TEST_F(GeicUrlAllowlistTest, DisallowedHosts) {
  EXPECT_FALSE(IsAllowedGeminiEnterpriseHost(GURL("https://example.com")));
  EXPECT_FALSE(
      IsAllowedGeminiEnterpriseHost(GURL("https://gemini.google.com/home")));
  EXPECT_FALSE(IsAllowedGeminiEnterpriseHost(GURL("https://localhost")));
  EXPECT_FALSE(
      IsAllowedGeminiEnterpriseHost(GURL("https://gemini.google/home")));
  EXPECT_FALSE(
      IsAllowedGeminiEnterpriseHost(GURL("https://sub.gemini.google/home")));
  EXPECT_FALSE(IsAllowedGeminiEnterpriseHost(
      GURL("https://sub.business.gemini.google/home")));
  EXPECT_FALSE(IsAllowedGeminiEnterpriseHost(
      GURL("https://sub.gemini.google.com/home")));
  EXPECT_FALSE(
      IsAllowedGeminiEnterpriseHost(GURL("https://cloud.google.com/home")));
  EXPECT_FALSE(IsAllowedGeminiEnterpriseHost(
      GURL("https://console.cloud.google.com/home")));
  EXPECT_FALSE(
      IsAllowedGeminiEnterpriseHost(GURL("https://foo.corp.google.com/")));
}

TEST_F(GeicUrlAllowlistTest, FinchGuestUrlDoesNotExpandAllowlist) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kGeic, {{features::kGeicGuestURL.name,
                         "https://custom-host.corp.google.com/side-panel"}});

  EXPECT_FALSE(IsAllowedGeminiEnterpriseHost(
      GURL("https://custom-host.corp.google.com/side-panel")));
  EXPECT_FALSE(IsAllowedGeminiEnterprisePreProductionHost(
      GURL("https://custom-host.corp.google.com/side-panel")));
}

// Regression guard: `IsAllowedGeminiEnterpriseHost()` is called by policy
// handlers, which can run before the FeatureList is initialized, so it must
// never read features.
TEST_F(GeicUrlAllowlistTest, BeforeFeatureListInit) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithNullFeatureAndFieldTrialLists();
  base::FeatureList::FailOnFeatureAccessWithoutFeatureList();

  EXPECT_FALSE(IsAllowedGeminiEnterpriseHost(GURL("https://example.com/home")));
  EXPECT_TRUE(IsAllowedGeminiEnterpriseHost(
      GURL("https://business.gemini.google/home")));
  EXPECT_TRUE(IsAllowedGeminiEnterprisePreProductionHost(
      GURL("https://business-staging.gemini.google/home")));
}

}  // namespace

}  // namespace geic
