// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_feature.h"
#include "chrome/browser/glic/public/glic_provider.h"
#include "chrome/browser/glic/public/glic_provider_spec.h"
#include "chrome/common/chrome_features.h"
#include "components/signin/public/base/signin_switches.h"
#include "pdf/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

TEST(GlicProviderSpecImplTest, GeminiSupportsSsrFollowsGlicSsr) {
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(features::kGlicSsr);
    EXPECT_TRUE(
        CreateGlicProviderSpec(GlicProvider::kGemini, {})->SupportsSsr());
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(features::kGlicSsr);
    EXPECT_FALSE(
        CreateGlicProviderSpec(GlicProvider::kGemini, {})->SupportsSsr());
  }
}

TEST(GlicProviderSpecImplTest, GeminiEnterpriseSupportsSsrFollowsGeicSsr) {
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures({features::kGeicSsr}, {features::kGlicSsr});
    EXPECT_TRUE(CreateGlicProviderSpec(GlicProvider::kGeminiEnterprise, {})
                    ->SupportsSsr());
  }
  // kGlicSsr only applies to consumer Gemini.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures({features::kGlicSsr}, {features::kGeicSsr});
    EXPECT_FALSE(CreateGlicProviderSpec(GlicProvider::kGeminiEnterprise, {})
                     ->SupportsSsr());
  }
}

TEST(GlicProviderSpecImplTest, OnlyGeminiSupportsOsEntryPoints) {
  EXPECT_TRUE(CreateGlicProviderSpec(GlicProvider::kGemini, {})
                  ->SupportsFeature(GlicFeature::kOsEntryPoints));
  EXPECT_FALSE(CreateGlicProviderSpec(GlicProvider::kGeminiEnterprise, {})
                   ->SupportsFeature(GlicFeature::kOsEntryPoints));
}

bool GeminiSupportsFeature(GlicFeature feature) {
  return CreateGlicProviderSpec(GlicProvider::kGemini, {})
      ->SupportsFeature(feature);
}

// Checks that consumer Gemini supports `feature` if and only if `flag` is
// enabled.
void ExpectGeminiFeatureFollowsFlag(GlicFeature feature,
                                    const base::Feature& flag) {
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(flag);
    EXPECT_TRUE(GeminiSupportsFeature(feature));
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(flag);
    EXPECT_FALSE(GeminiSupportsFeature(feature));
  }
}

TEST(GlicProviderSpecImplTest, GeminiSupportsLiveAndShareImageWithoutFlags) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {}, {features::kGlicLiveMode, features::kGlicShareImage});
  EXPECT_TRUE(GeminiSupportsFeature(GlicFeature::kLive));
  EXPECT_TRUE(GeminiSupportsFeature(GlicFeature::kShareImage));
}

TEST(GlicProviderSpecImplTest,
     GeminiSelectionInlineCueFollowsGlicSelectionPrompt) {
  ExpectGeminiFeatureFollowsFlag(GlicFeature::kSelectionInlineCue,
                                 features::kGlicSelectionPrompt);
}

TEST(GlicProviderSpecImplTest, GeminiCaptureRegionFollowsGlicCaptureRegion) {
  ExpectGeminiFeatureFollowsFlag(GlicFeature::kCaptureRegion,
                                 features::kGlicCaptureRegion);
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
TEST(GlicProviderSpecImplTest,
     GeminiFirstRunShowcaseFollowsFirstRunFeatureShowcaseGeminiStep) {
  ExpectGeminiFeatureFollowsFlag(GlicFeature::kFirstRunShowcase,
                                 switches::kFirstRunFeatureShowcaseGeminiStep);
}
#else
TEST(GlicProviderSpecImplTest, GeminiDoesNotSupportFirstRunShowcase) {
  EXPECT_FALSE(GeminiSupportsFeature(GlicFeature::kFirstRunShowcase));
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

TEST(GlicProviderSpecImplTest, GeminiPdfAutoOpenFollowsAutoOpenGlicForPdf) {
  ExpectGeminiFeatureFollowsFlag(GlicFeature::kPdfAutoOpen,
                                 features::kAutoOpenGlicForPdf);
}

#if BUILDFLAG(ENABLE_PDF)
TEST(GlicProviderSpecImplTest, GeminiPdfSummarizeFollowsPdfGlicSummarize) {
  ExpectGeminiFeatureFollowsFlag(GlicFeature::kPdfSummarize,
                                 features::kPdfGlicSummarize);
}
#else
TEST(GlicProviderSpecImplTest, GeminiDoesNotSupportPdfSummarize) {
  EXPECT_FALSE(GeminiSupportsFeature(GlicFeature::kPdfSummarize));
}
#endif  // BUILDFLAG(ENABLE_PDF)

TEST(GlicProviderSpecImplTest, GeminiEnterpriseSupportsNoFeatures) {
  std::vector<base::test::FeatureRef> enabled_features = {
      features::kGlicLiveMode,
      features::kGlicShareImage,
      features::kAutoOpenGlicForPdf,
      features::kGlicSelectionPrompt,
      features::kGlicCaptureRegion,
#if BUILDFLAG(ENABLE_PDF)
      features::kPdfGlicSummarize,
#endif
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
      switches::kFirstRunFeatureShowcaseGeminiStep,
#endif
  };
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(enabled_features, {});
  auto spec = CreateGlicProviderSpec(GlicProvider::kGeminiEnterprise, {});
  for (int i = 0; i <= static_cast<int>(GlicFeature::kMaxValue); ++i) {
    EXPECT_FALSE(spec->SupportsFeature(static_cast<GlicFeature>(i))) << i;
  }
}

}  // namespace
}  // namespace glic
