// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/provider_features.h"

#include <string>

#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/glic/public/glic_provider_spec.h"
#include "pdf/buildflags.h"

namespace features {

BASE_FEATURE(kAutoOpenGlicForPdf, base::FEATURE_DISABLED_BY_DEFAULT);
glic::GlicPdfAutoOpenFeatureParams PopulatePdfAutoOpenFeatureParams(
    const base::Feature& feature) {
  glic::GlicPdfAutoOpenFeatureParams defaults;
  return {
      .with_onboarding = base::GetFieldTrialParamByFeatureAsBool(
          feature, kAutoOpenGlicForPdfWithOnboardingParam,
          defaults.with_onboarding),
      .cooldown = base::GetFieldTrialParamByFeatureAsTimeDelta(
          feature, kAutoOpenGlicCooldownParam, defaults.cooldown),
  };
}

#if BUILDFLAG(ENABLE_PDF)
BASE_FEATURE(kPdfGlicSummarize, base::FEATURE_DISABLED_BY_DEFAULT);
glic::GlicPdfSummarizeFeatureParams PopulatePdfSummarizeFeatureParams(
    const base::Feature& feature) {
  glic::GlicPdfSummarizeFeatureParams defaults;
  return {
      .arm = base::GetFieldTrialParamByFeatureAsInt(
          feature, kPdfGlicSummarizeArmParam, defaults.arm),
      .use_long_button_text = base::GetFieldTrialParamByFeatureAsBool(
          feature, kPdfGlicSummarizeUseLongButtonTextParam,
          defaults.use_long_button_text),
  };
}
#endif  // BUILDFLAG(ENABLE_PDF)

BASE_FEATURE(kGlicSelectionPrompt, base::FEATURE_DISABLED_BY_DEFAULT);
glic::GlicSelectionInlineCueFeatureParams
PopulateSelectionInlineCueFeatureParams(const base::Feature& feature) {
  glic::GlicSelectionInlineCueFeatureParams params;
  params.show_copy_buttons = base::GetFieldTrialParamByFeatureAsBool(
      feature, kGlicSelectionShowCopyButtonsParam, params.show_copy_buttons);
  params.auto_send_prompt = base::GetFieldTrialParamByFeatureAsBool(
      feature, kGlicSelectionAutoSendPromptParam, params.auto_send_prompt);
  params.cta = base::GetFieldTrialParamByFeatureAsString(
      feature, kGlicSelectionCtaParam, params.cta);
  std::string sites_str = base::GetFieldTrialParamByFeatureAsString(
      feature, kGlicSelectionDefaultBlockedSitesParam, "");
  params.default_blocked_sites = base::flat_set<std::string>(
      base::SplitString(sites_str, ",", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY));
  return params;
}

BASE_FEATURE(kGlicCaptureRegion,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
BASE_FEATURE(kFirstRunFeatureShowcaseGeminiStep,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

}  // namespace features
