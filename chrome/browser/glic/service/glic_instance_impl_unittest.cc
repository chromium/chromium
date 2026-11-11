// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_instance_impl.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/public/glic_provider.h"
#include "chrome/browser/glic/public/glic_provider_spec.h"
#include "chrome/browser/glic/public/provider_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace glic {
namespace {

TEST(GlicInstanceImplTest, IsTutorialMarketingUrl_PromotionPage) {
  auto spec = CreateGlicProviderSpec(GlicProvider::kGemini, {});

  EXPECT_TRUE(GlicInstanceImpl::IsTutorialMarketingUrl(
      GURL("https://www.google.com/chrome/ai-innovations/gemini-in-chrome/"
           "?source=zss#gemini"),
      *spec));
  EXPECT_TRUE(GlicInstanceImpl::IsTutorialMarketingUrl(
      GURL("https://www.google.com/chrome/ai-innovations/gemini-in-chrome"
           "?source=zss"),
      *spec));
  EXPECT_TRUE(GlicInstanceImpl::IsTutorialMarketingUrl(
      GURL("https://www.google.com/chrome/ai-innovations/gemini-in-chrome/"
           "?utm_medium=cta&source=zss#gemini"),
      *spec));

  // Missing or mismatched source query parameter.
  EXPECT_FALSE(GlicInstanceImpl::IsTutorialMarketingUrl(
      GURL("https://www.google.com/chrome/ai-innovations/gemini-in-chrome/"),
      *spec));
  EXPECT_FALSE(GlicInstanceImpl::IsTutorialMarketingUrl(
      GURL("https://www.google.com/chrome/ai-innovations/gemini-in-chrome/"
           "?source=other"),
      *spec));

  // Requires HTTPS scheme.
  EXPECT_FALSE(GlicInstanceImpl::IsTutorialMarketingUrl(
      GURL("http://www.google.com/chrome/ai-innovations/gemini-in-chrome/"
           "?source=zss"),
      *spec));

  // Requires exact www.google.com host and exact path.
  EXPECT_FALSE(GlicInstanceImpl::IsTutorialMarketingUrl(
      GURL("https://sites.google.com/view/foo/chrome/ai-innovations/"
           "gemini-in-chrome?source=zss"),
      *spec));
  EXPECT_FALSE(GlicInstanceImpl::IsTutorialMarketingUrl(
      GURL("https://www.google.com/intl/en/chrome/ai-innovations/"
           "gemini-in-chrome/?source=zss"),
      *spec));
  EXPECT_FALSE(GlicInstanceImpl::IsTutorialMarketingUrl(
      GURL("https://www.google.com/chrome/ai-innovations/gemini-in-chrome/"
           "extra/?source=zss"),
      *spec));

  // Invalid and non-HTTP(S) URLs.
  EXPECT_FALSE(GlicInstanceImpl::IsTutorialMarketingUrl(GURL(), *spec));
  EXPECT_FALSE(GlicInstanceImpl::IsTutorialMarketingUrl(
      GURL("chrome://settings/?source=zss"), *spec));
}

TEST(GlicInstanceImplTest, IsTutorialMarketingUrl_ProviderSpecAllowlist) {
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::kGlicMarketingAutoOpen,
        {{features::kGlicMarketingAutoOpenAllowlistedUrlsParam,
          "https://example.com/tutorial/, http://127.0.0.1:8080/title1.html"}});
    auto spec = CreateGlicProviderSpec(GlicProvider::kGemini, {});

    EXPECT_TRUE(GlicInstanceImpl::IsTutorialMarketingUrl(
        GURL("https://example.com/tutorial?source=zss#section"), *spec));
    EXPECT_TRUE(GlicInstanceImpl::IsTutorialMarketingUrl(
        GURL("http://127.0.0.1:8080/title1.html/?utm_source=x&source=zss"),
        *spec));

    EXPECT_FALSE(GlicInstanceImpl::IsTutorialMarketingUrl(
        GURL("https://example.com/tutorial"), *spec));
    EXPECT_FALSE(GlicInstanceImpl::IsTutorialMarketingUrl(
        GURL("http://example.com/tutorial?source=zss"), *spec));
    EXPECT_FALSE(GlicInstanceImpl::IsTutorialMarketingUrl(
        GURL("http://127.0.0.1:9090/title1.html?source=zss"), *spec));
    EXPECT_FALSE(GlicInstanceImpl::IsTutorialMarketingUrl(
        GURL("https://example.com/other?source=zss"), *spec));
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(features::kGlicMarketingAutoOpen);
    auto spec = CreateGlicProviderSpec(GlicProvider::kGemini, {});

    EXPECT_FALSE(GlicInstanceImpl::IsTutorialMarketingUrl(
        GURL("https://example.com/tutorial?source=zss"), *spec));
    EXPECT_TRUE(GlicInstanceImpl::IsTutorialMarketingUrl(
        GURL("https://www.google.com/chrome/ai-innovations/gemini-in-chrome/"
             "?source=zss#gemini"),
        *spec));
  }
}

TEST(GlicInstanceImplTest,
     IsTutorialMarketingUrl_DisabledWithoutGeminiAccountEligibility) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kGlicMarketingAutoOpen,
      {{features::kGlicMarketingAutoOpenAllowlistedUrlsParam,
        "https://example.com/tutorial"}});
  auto geic_spec = CreateGlicProviderSpec(GlicProvider::kGeminiEnterprise, {});

  EXPECT_FALSE(GlicInstanceImpl::IsTutorialMarketingUrl(
      GURL("https://www.google.com/chrome/ai-innovations/gemini-in-chrome/"
           "?source=zss#gemini"),
      *geic_spec));
  EXPECT_FALSE(GlicInstanceImpl::IsTutorialMarketingUrl(
      GURL("https://example.com/tutorial?source=zss"), *geic_spec));
}

}  // namespace
}  // namespace glic
