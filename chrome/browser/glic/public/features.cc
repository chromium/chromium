// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/features.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"

namespace features {

BASE_FEATURE(kGlicTabRestoration, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kGlicChromeStatusIcon, base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<int> kGlicChromeStatusIconSizePx{
    &kGlicChromeStatusIcon, "glic-chrome-status-icon-size-px", 20};

BASE_FEATURE(kGlicOrphanedReattachment, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kGlicSelectionPrompt, base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kGlicDaisyChainViaCoordinator, base::FEATURE_ENABLED_BY_DEFAULT);
#else
BASE_FEATURE(kGlicDaisyChainViaCoordinator, base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kAutoOpenGlicForPdf, base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<bool> kAutoOpenGlicForPdfWithOnboarding({
    &kAutoOpenGlicForPdf,
    "AutoOpenGlicForPdfWithOnboarding",
    false,
});

BASE_FEATURE(kGlicInvoke, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kGlicSummarizeVideoSuggestion, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kGlicContextMenu, base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<std::string> kGlicContextMenuArm{&kGlicContextMenu,
                                                          "variant", "arm1"};
const base::FeatureParam<bool> kGlicContextMenuWithOnboarding{
    &kGlicContextMenu, "WithOnboarding", false};

BASE_FEATURE(kGlicFixTimeToFirstQueryKillSwitch,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kGlicTieredRolloutV2, base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<std::string> kGlicTieredRolloutV2EligibleTiers{
    &kGlicTieredRolloutV2, "glic-tiered-rollout-v2-eligible-tiers", ""};
const base::flat_set<int32_t>& GetGlicTieredRolloutV2EligibleTiers() {
  static const base::NoDestructor<base::flat_set<int32_t>> eligible_tiers([] {
    std::string tier_list = kGlicTieredRolloutV2EligibleTiers.Get();
    std::vector<std::string_view> tier_pieces = base::SplitStringPiece(
        tier_list, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    base::flat_set<int32_t> tiers;
    tiers.reserve(tier_pieces.size());
    for (const auto& piece : tier_pieces) {
      int32_t tier_id = 0;
      if (base::StringToInt(piece, &tier_id)) {
        tiers.insert(tier_id);
      }
    }
    return tiers;
  }());
  return *eligible_tiers;
}

}  // namespace features
