// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_PROVIDER_FEATURES_H_
#define CHROME_BROWSER_GLIC_PUBLIC_PROVIDER_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "pdf/buildflags.h"

// Feature flags and parameters whose availability depends on the Glic provider.
// Production code must query GlicProviderSpec instead; only the spec, tests,
// and flag registration may include this file.

namespace glic {
struct GlicIndigoFeatureParams;
struct GlicMarketingAutoOpenFeatureParams;
struct GlicPdfAutoOpenFeatureParams;
struct GlicPdfSummarizeFeatureParams;
struct GlicSelectionInlineCueFeatureParams;
}  // namespace glic

namespace features {

// Auto-opening Glic for PDFs. See GlicFeature::kPdfAutoOpen.
BASE_DECLARE_FEATURE(kAutoOpenGlicForPdf);
inline constexpr char kAutoOpenGlicForPdfWithOnboardingParam[] =
    "AutoOpenGlicForPdfWithOnboarding";
inline constexpr char kAutoOpenGlicCooldownParam[] = "AutoOpenGlicCooldown";
glic::GlicPdfAutoOpenFeatureParams PopulatePdfAutoOpenFeatureParams(
    const base::Feature& feature);

#if BUILDFLAG(ENABLE_PDF)
// The PDF viewer's summarize button and its API. See
// GlicFeature::kPdfSummarize.
BASE_DECLARE_FEATURE(kPdfGlicSummarize);
inline constexpr char kPdfGlicSummarizeArmParam[] = "arm";
inline constexpr char kPdfGlicSummarizeUseLongButtonTextParam[] =
    "use_long_button_text";
glic::GlicPdfSummarizeFeatureParams PopulatePdfSummarizeFeatureParams(
    const base::Feature& feature);
#endif  // BUILDFLAG(ENABLE_PDF)

// The inline cue shown for text selections. See
// GlicFeature::kSelectionInlineCue.
BASE_DECLARE_FEATURE(kGlicSelectionPrompt);
inline constexpr char kGlicSelectionShowCopyButtonsParam[] =
    "show_copy_buttons";
inline constexpr char kGlicSelectionAutoSendPromptParam[] = "auto_send_prompt";
inline constexpr char kGlicSelectionCtaParam[] = "cta";
inline constexpr char kGlicSelectionDefaultBlockedSitesParam[] =
    "GlicSelectionDefaultBlockedSites";
glic::GlicSelectionInlineCueFeatureParams
PopulateSelectionInlineCueFeatureParams(const base::Feature& feature);

// Capturing a page region via the region selection overlay. See
// GlicFeature::kCaptureRegion.
BASE_DECLARE_FEATURE(kGlicCaptureRegion);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// The Gemini step of the first run feature showcase. This feature is a no-op
// unless the first run desktop revamp is enabled. See
// GlicFeature::kFirstRunShowcase.
//
// Note: This flag can be bypassed via the command line switch
// --force-fre-feature-showcase-steps (which should be used only for testing).
BASE_DECLARE_FEATURE(kFirstRunFeatureShowcaseGeminiStep);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

// Auto-opening Glic on allowlisted marketing pages. See
// GlicFeature::kMarketingAutoOpen.
BASE_DECLARE_FEATURE(kGlicMarketingAutoOpen);
inline constexpr char kGlicMarketingAutoOpenAllowlistedUrlsParam[] =
    "allowlisted_urls";
inline constexpr char kGlicMarketingAutoOpenMaxImpressionsParam[] =
    "max_impressions";
glic::GlicMarketingAutoOpenFeatureParams PopulateMarketingAutoOpenFeatureParams(
    const base::Feature& feature);

// Opening Glic from the Indigo page action. See GlicFeature::kIndigoOpenGlic.
BASE_DECLARE_FEATURE(kIndigoOpenGlic);
inline constexpr char kIndigoGlicPromptParam[] = "indigo_glic_prompt";
inline constexpr char kIndigoGlicPromptKeyParam[] = "indigo_glic_prompt_key";
inline constexpr char kIndigoGlicSkillIdParam[] = "indigo_glic_skill_id";
inline constexpr char kIndigoGlicTriggerDelayParam[] =
    "indigo_glic_trigger_delay";
glic::GlicIndigoFeatureParams PopulateIndigoFeatureParams(
    const base::Feature& feature);

}  // namespace features

#endif  // CHROME_BROWSER_GLIC_PUBLIC_PROVIDER_FEATURES_H_
