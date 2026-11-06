// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_feature.h"
#include "chrome/browser/glic/public/glic_provider.h"
#include "chrome/browser/glic/public/glic_provider_spec.h"
#include "chrome/browser/glic/public/provider_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/skills/features.h"
#include "pdf/buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

TEST(GlicProviderSpecImplTest, PromoBodyStringId) {
  EXPECT_EQ(CreateGlicProviderSpec(GlicProvider::kGemini, {})
                ->GetPromoBodyStringId(),
            IDS_GLIC_PROMO_BODY);
  EXPECT_EQ(CreateGlicProviderSpec(GlicProvider::kGeminiEnterprise, {})
                ->GetPromoBodyStringId(),
            IDS_GEIC_PROMO_BODY);
}

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
     GeminiExperimentalTriggeringFollowsGlicExperimentalTriggering) {
  ExpectGeminiFeatureFollowsFlag(GlicFeature::kExperimentalTriggering,
                                 features::kGlicExperimentalTriggering);
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
                                 features::kFirstRunFeatureShowcaseGeminiStep);
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

TEST(GlicProviderSpecImplTest,
     OnlyGeminiSupportsContextualCueingAndZeroStateSuggestions) {
  for (GlicFeature feature :
       {GlicFeature::kContextualCueing, GlicFeature::kZeroStateSuggestions}) {
    EXPECT_TRUE(GeminiSupportsFeature(feature));
    EXPECT_FALSE(CreateGlicProviderSpec(GlicProvider::kGeminiEnterprise, {})
                     ->SupportsFeature(feature));
  }
}

TEST(GlicProviderSpecImplTest, GeminiSkillsFollowsSkillsEnabled) {
  ExpectGeminiFeatureFollowsFlag(GlicFeature::kSkills,
                                 ::features::kSkillsEnabled);
}

TEST(GlicProviderSpecImplTest, GeminiIndigoFollowsIndigoOpenGlic) {
  ExpectGeminiFeatureFollowsFlag(GlicFeature::kIndigoOpenGlic,
                                 features::kIndigoOpenGlic);
}

TEST(GlicProviderSpecImplTest,
     GeminiMarketingAutoOpenFollowsGlicMarketingAutoOpen) {
  ExpectGeminiFeatureFollowsFlag(GlicFeature::kMarketingAutoOpen,
                                 features::kGlicMarketingAutoOpen);
}

TEST(GlicProviderSpecImplTest, GeminiEnterpriseSupportsNoFeatures) {
  std::vector<base::test::FeatureRef> enabled_features = {
      features::kGlicLiveMode,
      features::kGlicShareImage,
      features::kGlicExperimentalTriggering,
      features::kAutoOpenGlicForPdf,
      features::kGlicSelectionPrompt,
      features::kGlicCaptureRegion,
      ::features::kSkillsEnabled,
      features::kIndigoOpenGlic,
      features::kGlicMarketingAutoOpen,
#if BUILDFLAG(ENABLE_PDF)
      features::kPdfGlicSummarize,
#endif
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
      features::kFirstRunFeatureShowcaseGeminiStep,
#endif
  };
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(enabled_features, {});
  auto spec = CreateGlicProviderSpec(GlicProvider::kGeminiEnterprise, {});
  for (int i = 0; i <= static_cast<int>(GlicFeature::kMaxValue); ++i) {
    EXPECT_FALSE(spec->SupportsFeature(static_cast<GlicFeature>(i))) << i;
  }
}

TEST(GlicProviderSpecImplTest, PdfAutoOpenFeatureParams) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kAutoOpenGlicForPdf,
      {{features::kAutoOpenGlicForPdfWithOnboardingParam, "false"},
       {features::kAutoOpenGlicCooldownParam, "30m"}});

  GlicPdfAutoOpenFeatureParams gemini_params =
      CreateGlicProviderSpec(GlicProvider::kGemini, {})
          ->GetPdfAutoOpenFeatureParams();
  EXPECT_FALSE(gemini_params.with_onboarding);
  EXPECT_EQ(gemini_params.cooldown, base::Minutes(30));

  GlicPdfAutoOpenFeatureParams geic_params =
      CreateGlicProviderSpec(GlicProvider::kGeminiEnterprise, {})
          ->GetPdfAutoOpenFeatureParams();
  EXPECT_TRUE(geic_params.with_onboarding);
  EXPECT_EQ(geic_params.cooldown, base::Hours(1));
}

#if BUILDFLAG(ENABLE_PDF)
TEST(GlicProviderSpecImplTest, PdfSummarizeFeatureParams) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kPdfGlicSummarize,
      {{features::kPdfGlicSummarizeArmParam, "2"},
       {features::kPdfGlicSummarizeUseLongButtonTextParam, "true"}});

  GlicPdfSummarizeFeatureParams gemini_params =
      CreateGlicProviderSpec(GlicProvider::kGemini, {})
          ->GetPdfSummarizeFeatureParams();
  EXPECT_EQ(gemini_params.arm, 2);
  EXPECT_TRUE(gemini_params.use_long_button_text);

  GlicPdfSummarizeFeatureParams geic_params =
      CreateGlicProviderSpec(GlicProvider::kGeminiEnterprise, {})
          ->GetPdfSummarizeFeatureParams();
  EXPECT_EQ(geic_params.arm, 3);
  EXPECT_FALSE(geic_params.use_long_button_text);
}
#else
TEST(GlicProviderSpecImplTest, PdfSummarizeFeatureParamsWithoutPdf) {
  GlicPdfSummarizeFeatureParams gemini_params =
      CreateGlicProviderSpec(GlicProvider::kGemini, {})
          ->GetPdfSummarizeFeatureParams();
  EXPECT_EQ(gemini_params.arm, 3);
  EXPECT_FALSE(gemini_params.use_long_button_text);
}
#endif  // BUILDFLAG(ENABLE_PDF)

TEST(GlicProviderSpecImplTest, SelectionInlineCueFeatureParams) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kGlicSelectionPrompt,
      {{features::kGlicSelectionShowCopyButtonsParam, "true"},
       {features::kGlicSelectionAutoSendPromptParam, "false"},
       {features::kGlicSelectionCtaParam,
        GlicSelectionInlineCueFeatureParams::kCtaTellMe},
       {features::kGlicSelectionDefaultBlockedSitesParam,
        "example.com, foo.org"}});

  GlicSelectionInlineCueFeatureParams gemini_params =
      CreateGlicProviderSpec(GlicProvider::kGemini, {})
          ->GetSelectionInlineCueFeatureParams();
  EXPECT_TRUE(gemini_params.show_copy_buttons);
  EXPECT_FALSE(gemini_params.auto_send_prompt);
  EXPECT_EQ(gemini_params.cta, GlicSelectionInlineCueFeatureParams::kCtaTellMe);
  EXPECT_THAT(gemini_params.default_blocked_sites,
              testing::UnorderedElementsAre("example.com", "foo.org"));

  GlicSelectionInlineCueFeatureParams geic_params =
      CreateGlicProviderSpec(GlicProvider::kGeminiEnterprise, {})
          ->GetSelectionInlineCueFeatureParams();
  EXPECT_FALSE(geic_params.show_copy_buttons);
  EXPECT_TRUE(geic_params.auto_send_prompt);
  EXPECT_EQ(geic_params.cta, GlicSelectionInlineCueFeatureParams::kCtaExplain);
  EXPECT_TRUE(geic_params.default_blocked_sites.empty());
}

}  // namespace
}  // namespace glic
