// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/features.h"

#include "base/feature.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "build/android_buildflags.h"
#include "build/build_config.h"

namespace features {

BASE_FEATURE(kGlicAndroidSidePanel, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kGlicChromeStatusIcon, base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<int> kGlicChromeStatusIconSizePx{
    &kGlicChromeStatusIcon, "glic-chrome-status-icon-size-px", 20};
const base::FeatureParam<bool> kGlicChromeStatusIconUseAltIcon{
    &kGlicChromeStatusIcon, "glic-chrome-status-icon-use-alt-icon", false};
const base::FeatureParam<bool> kGlicChromeStatusIconLogOnly{
    &kGlicChromeStatusIcon, "glic-chrome-status-icon-log-only", true};
const base::FeatureParam<std::string> kGlicChromeStatusIconOtherAppID{
    &kGlicChromeStatusIcon, "glic-chrome-status-icon-other-app-id", ""};

BASE_FEATURE(kGlicOSIconVariant, base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<int> kGlicOSIconVariantParam{&kGlicOSIconVariant,
                                                      "variant", 0};

BASE_FEATURE(kGlicOrphanedReattachment, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kGlicSelectionPrompt, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<bool> kGlicSelectionPromptUpdatesOnly{
    &kGlicSelectionPrompt, "updates_only", false};
const base::FeatureParam<bool> kGlicSelectionPromptUseWidget{
    &kGlicSelectionPrompt, "use_widget", true};
const base::FeatureParam<bool> kGlicSelectionPromptEnablePinning{
    &kGlicSelectionPrompt, "enable_pinning", false};
const base::FeatureParam<std::string> kGlicSelectionTopCueOnlyList{
    &kGlicSelectionPrompt, "top_cue_only_list", ""};
const base::FeatureParam<int> kGlicSelectionPromptWidgetMaxTotalDismisses{
    &kGlicSelectionPrompt, "max_total_dismisses", 10};

BASE_FEATURE(kGlicClearTurnIdOnPanelWillOpen,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAutoOpenGlicForPdf, base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<bool> kAutoOpenGlicForPdfWithOnboarding({
    &kAutoOpenGlicForPdf,
    "AutoOpenGlicForPdfWithOnboarding",
    true,
});
const base::FeatureParam<base::TimeDelta> kAutoOpenGlicCooldown({
    &kAutoOpenGlicForPdf,
    "AutoOpenGlicCooldown",
    base::Hours(1),
});

BASE_FEATURE(kGlicInvoke, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGlicOnboardingMetricsMigration, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kGlicCreateTabAdjacent, base::FEATURE_ENABLED_BY_DEFAULT);

// When off, disables both live mode and the glic floating panel.
BASE_FEATURE(kGlicLiveMode,
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kGlicDefaultToLastActiveConversation,
#if BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_DESKTOP_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

const base::FeatureParam<base::TimeDelta>
    kGlicDefaultToLastActiveConversationMaxRecency{
        &kGlicDefaultToLastActiveConversation, "max_recency",
        base::Minutes(20)};

BASE_FEATURE(kGlicSummarizeVideoSuggestion, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kGlicFixTimeToFirstQueryKillSwitch,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kGlicContextMenu, base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<std::string> kGlicContextMenuArm{&kGlicContextMenu,
                                                          "variant", "arm1"};
const base::FeatureParam<bool> kGlicContextMenuWithOnboarding{
    &kGlicContextMenu, "WithOnboarding", false};

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

BASE_FEATURE(kGlicHorizontalTabToolbarButton,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kGlicToolbarButtonLocation, base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<GlicToolbarButtonLocation>::Option
    kGlicButtonLocationOptions[] = {
        {GlicToolbarButtonLocation::kRightOfOmnibox,
         kGlicToolbarButtonLocationRightOfOmnibox},
        {GlicToolbarButtonLocation::kLeftOfProfileChip,
         kGlicToolbarButtonLocationLeftOfProfileChip},
        {GlicToolbarButtonLocation::kLeftOfProfileChipWithBackground,
         kGlicToolbarButtonLocationLeftOfProfileChipWithBackground}};

const base::FeatureParam<GlicToolbarButtonLocation>
    kGlicToolbarButtonLocationParam{
        &kGlicToolbarButtonLocation, "glic-toolbar-button-location",
        GlicToolbarButtonLocation::kLeftOfProfileChip,
        &kGlicButtonLocationOptions};

BASE_FEATURE(kGlicButtonAutoSummarize, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kGlicGetTabFaviconById, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kGlicSkipCookieSyncOnOpen, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kGlicCookieSyncOnTokenChange, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kGlicShareImageViaInvoke, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kGlicWebClientLoadTimes, base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<int> kGlicPreLoadingTimeMs{
    &kGlicWebClientLoadTimes, "glic-pre-loading-time-ms", 200};
const base::FeatureParam<int> kGlicMinLoadingTimeMs{
    &kGlicWebClientLoadTimes, "glic-min-loading-time-ms", 1000};
const base::FeatureParam<int> kGlicMaxLoadingTimeMs{
    &kGlicWebClientLoadTimes, "glic-max-loading-time-ms", 20000};
const base::FeatureParam<int> kGlicReloadMaxLoadingTimeMs{
    &kGlicWebClientLoadTimes, "glic-reload-max-loading-time-ms", 30000};

BASE_FEATURE(kGlicContextualCueingV2AutoSubmit,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kGlicWebDragAndDropFileUpload, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kGlicOptInImpressionMetrics, base::FEATURE_ENABLED_BY_DEFAULT);

// Killswitch that controls whether the WebContents visibility state is
// set to hidden when the Glic panel is warming.
// TODO(crbug.com/513620671) Investigate enabling on Windows.
// TODO(crbug.com/516381993) Investigate enabling on ChromeOS.
BASE_FEATURE(kGlicContentsInitiallyHidden,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kGlicAnchorEntryPointForOnboardedUsers,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If kGlicShowForSignedOut is enabled, the GiC panel can be shown to signed out
// users to show the sign-in promotion.
BASE_FEATURE(kGlicShowForSignedOut,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Killswitch that controls whether to update the WebContents visibility state
// when toggling the Glic panel.
BASE_FEATURE(kGlicSetWebContentsVisibilityWhenToggling,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kGlicProcessCounterAbuseVerdict,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kGlicNoWebUiLoader, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kGlicGeminiEnterpriseSettingsEnabled,
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features
