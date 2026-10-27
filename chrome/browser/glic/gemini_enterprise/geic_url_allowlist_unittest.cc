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
  EXPECT_TRUE(
      IsAllowedGeminiEnterpriseHost(GURL("https://gemini.google/home")));
  EXPECT_TRUE(IsAllowedGeminiEnterpriseHost(
      GURL("https://business.gemini.google/home")));
  EXPECT_TRUE(
      IsAllowedGeminiEnterpriseHost(GURL("https://gemini.google.com/home")));
  EXPECT_TRUE(IsAllowedGeminiEnterpriseHost(
      GURL("https://enterprise.gemini.cloud.google/home")));
  EXPECT_TRUE(IsAllowedGeminiEnterpriseHost(
      GURL("https://enterprise.gemini.cloud.google.com/home")));
  EXPECT_TRUE(
      IsAllowedGeminiEnterpriseHost(GURL("https://gemini.cloud.google/home")));
  EXPECT_TRUE(IsAllowedGeminiEnterpriseHost(
      GURL("https://vertexaisearch.cloud.google/home")));
  EXPECT_TRUE(IsAllowedGeminiEnterpriseHost(
      GURL("https://vertexaisearch.cloud.google.com/home")));
}

TEST_F(GeicUrlAllowlistTest, DisallowedHosts) {
  EXPECT_FALSE(IsAllowedGeminiEnterpriseHost(GURL("https://example.com")));
  EXPECT_FALSE(IsAllowedGeminiEnterpriseHost(GURL("https://localhost")));
}

TEST_F(GeicUrlAllowlistTest, FinchGuestUrlAllowed) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kGeic, {{features::kGeicGuestURL.name,
                         "https://custom-host.corp.google.com/side-panel"}});

  EXPECT_TRUE(IsAllowedGeminiEnterpriseHost(
      GURL("https://custom-host.corp.google.com/other")));
  EXPECT_FALSE(
      IsAllowedGeminiEnterpriseHost(GURL("https://other.corp.google.com/")));
}

TEST_F(GeicUrlAllowlistTest, BeforeFeatureListInit) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithNullFeatureAndFieldTrialLists();
  base::FeatureList::FailOnFeatureAccessWithoutFeatureList();

  EXPECT_FALSE(IsAllowedGeminiEnterpriseHost(GURL("https://example.com/home")));
  EXPECT_TRUE(IsAllowedGeminiEnterpriseHost(
      GURL("https://business.gemini.google/home")));
}

}  // namespace

}  // namespace geic
