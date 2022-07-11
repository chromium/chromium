// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_budget/active_sampling.h"

#include "base/i18n/case_conversion.h"
#include "base/location.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/common/privacy_budget/field_trial_param_conversions.h"
#include "chrome/common/privacy_budget/privacy_budget_features.h"
#include "content/public/common/user_agent.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_sample_collector.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace {

void ActivelySampleUserAgentModel() {
  ukm::SourceId ukm_source_id = ukm::NoURLSourceId();
  content::BuildModelInfo();
  auto identifiable_surface = blink::IdentifiableSurface::FromTypeAndToken(
      blink::IdentifiableSurface::Type::kNavigatorUAData_GetHighEntropyValues,
      blink::IdentifiableToken("model"));
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  if (!ukm_recorder) {
    return;
  }
  blink::IdentifiabilityMetricBuilder(ukm_source_id)
      .Add(identifiable_surface,
           blink::IdentifiableToken(content::BuildModelInfo()))
      .Record(ukm_recorder);
  blink::IdentifiabilitySampleCollector::Get()->FlushSource(ukm_recorder,
                                                            ukm_source_id);
}

bool IsFontFamilyAvailable(const char* family, SkFontMgr* fm) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  return !!fm->legacyMakeTypeface(family, SkFontStyle());
#else
  sk_sp<SkFontStyleSet> set(fm->matchFamily(family));
  return set && set->count();
#endif
}

void ReportAvailableFontFamilies(std::vector<std::string> fonts_to_check) {
  sk_sp<SkFontMgr> fontManager(SkFontMgr::RefDefault());
  ukm::SourceId ukm_source_id = ukm::NoURLSourceId();
  blink::IdentifiabilityMetricBuilder builder(ukm_source_id);
  for (const std::string& font : fonts_to_check) {
    bool is_available = IsFontFamilyAvailable(font.c_str(), fontManager.get());

    // Compute a case-insensitive (in a unicode-compatible way) hash for the
    // surface key.
    blink::IdentifiableToken font_family_name_token(
        base::UTF16ToUTF8(base::i18n::FoldCase(base::UTF8ToUTF16(font))));
    builder.Add(blink::IdentifiableSurface::FromTypeAndToken(
                    blink::IdentifiableSurface::Type::kFontFamilyAvailable,
                    font_family_name_token),
                is_available);
  }
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  if (!ukm_recorder) {
    return;
  }
  builder.Record(ukm_recorder);
  blink::IdentifiabilitySampleCollector::Get()->FlushSource(ukm_recorder,
                                                            ukm_source_id);
}

void ActivelySampleAvailableFonts() {
  std::vector<std::string> font_families =
      DecodeIdentifiabilityFieldTrialParam<std::vector<std::string>>(
          features::kIdentifiabilityStudyActivelySampledFonts.Get());

  if (font_families.empty())
    return;

  // Checking font availability can be expensive and might block.
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&ReportAvailableFontFamilies, std::move(font_families)));
}

}  // namespace

void ActivelySampleIdentifiableSurfaces() {
  if (!blink::IdentifiabilityStudySettings::Get()->IsActive())
    return;
  ActivelySampleUserAgentModel();
  ActivelySampleAvailableFonts();
}
