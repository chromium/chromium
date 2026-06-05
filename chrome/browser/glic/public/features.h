// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_FEATURES_H_
#define CHROME_BROWSER_GLIC_PUBLIC_FEATURES_H_

#include <string>

#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace features {

BASE_DECLARE_FEATURE(kGlicAndroidSidePanel);

BASE_DECLARE_FEATURE(kGlicClearTurnIdOnPanelWillOpen);
BASE_DECLARE_FEATURE(kGlicChromeStatusIcon);
extern const base::FeatureParam<int> kGlicChromeStatusIconSizePx;
extern const base::FeatureParam<bool> kGlicChromeStatusIconUseAltIcon;
extern const base::FeatureParam<bool> kGlicChromeStatusIconLogOnly;
extern const base::FeatureParam<std::string> kGlicChromeStatusIconOtherAppID;

BASE_DECLARE_FEATURE(kGlicOSIconVariant);
extern const base::FeatureParam<int> kGlicOSIconVariantParam;

BASE_DECLARE_FEATURE(kGlicOrphanedReattachment);

BASE_DECLARE_FEATURE(kAutoOpenGlicForPdf);
extern const base::FeatureParam<bool> kAutoOpenGlicForPdfWithOnboarding;
extern const base::FeatureParam<base::TimeDelta> kAutoOpenGlicCooldown;

BASE_DECLARE_FEATURE(kGlicInvoke);
BASE_DECLARE_FEATURE(kGlicOnboardingMetricsMigration);

BASE_DECLARE_FEATURE(kGlicSelectionPrompt);
extern const base::FeatureParam<bool> kGlicSelectionPromptUpdatesOnly;
extern const base::FeatureParam<bool> kGlicSelectionPromptUseWidget;
extern const base::FeatureParam<bool> kGlicSelectionPromptEnablePinning;
extern const base::FeatureParam<std::string> kGlicSelectionTopCueOnlyList;
extern const base::FeatureParam<int>
    kGlicSelectionPromptWidgetMaxTotalDismisses;

BASE_DECLARE_FEATURE(kGlicCreateTabAdjacent);

BASE_DECLARE_FEATURE(kGlicLiveMode);

BASE_DECLARE_FEATURE(kGlicDefaultToLastActiveConversation);
extern const base::FeatureParam<base::TimeDelta>
    kGlicDefaultToLastActiveConversationMaxRecency;

BASE_DECLARE_FEATURE(kGlicSummarizeVideoSuggestion);

BASE_DECLARE_FEATURE(kGlicFixTimeToFirstQueryKillSwitch);

BASE_DECLARE_FEATURE(kGlicContextMenu);
extern const base::FeatureParam<std::string> kGlicContextMenuArm;
extern const base::FeatureParam<bool> kGlicContextMenuWithOnboarding;

BASE_DECLARE_FEATURE(kGlicTieredRolloutV2);
extern const base::FeatureParam<std::string> kGlicTieredRolloutV2EligibleTiers;
const base::flat_set<int32_t>& GetGlicTieredRolloutV2EligibleTiers();

BASE_DECLARE_FEATURE(kGlicHorizontalTabToolbarButton);

enum class GlicToolbarButtonLocation {
  kRightOfOmnibox,
  kLeftOfProfileChip,
  kLeftOfProfileChipWithBackground,
};
BASE_DECLARE_FEATURE(kGlicToolbarButtonLocation);
extern const base::FeatureParam<GlicToolbarButtonLocation>
    kGlicToolbarButtonLocationParam;

// String constants for GlicToolbarButtonLocation.
inline constexpr char kGlicToolbarButtonLocationRightOfOmnibox[] =
    "RightOfOmnibox";
inline constexpr char kGlicToolbarButtonLocationLeftOfProfileChip[] =
    "LeftOfProfileChip";
inline constexpr char
    kGlicToolbarButtonLocationLeftOfProfileChipWithBackground[] =
        "LeftOfProfileChipWithBackground";

BASE_DECLARE_FEATURE(kGlicButtonAutoSummarize);

BASE_DECLARE_FEATURE(kGlicGetTabFaviconById);

BASE_DECLARE_FEATURE(kGlicSkipCookieSyncOnOpen);
BASE_DECLARE_FEATURE(kGlicCookieSyncOnTokenChange);
BASE_DECLARE_FEATURE(kGlicCookieSyncOnError);
extern const base::FeatureParam<base::TimeDelta>
    kGlicCookieSyncOnErrorMinInterval;
BASE_DECLARE_FEATURE(kGlicShareImageViaInvoke);

BASE_DECLARE_FEATURE(kGlicWebClientLoadTimes);
extern const base::FeatureParam<int> kGlicPreLoadingTimeMs;
extern const base::FeatureParam<int> kGlicMinLoadingTimeMs;
extern const base::FeatureParam<int> kGlicMaxLoadingTimeMs;
extern const base::FeatureParam<int> kGlicReloadMaxLoadingTimeMs;

BASE_DECLARE_FEATURE(kGlicContextualCueingV2AutoSubmit);

BASE_DECLARE_FEATURE(kGlicWebDragAndDropFileUpload);

BASE_DECLARE_FEATURE(kGlicOptInImpressionMetrics);

BASE_DECLARE_FEATURE(kGlicContentsInitiallyHidden);
BASE_DECLARE_FEATURE(kGlicShowForSignedOut);

BASE_DECLARE_FEATURE(kGlicSetWebContentsVisibilityWhenToggling);

BASE_DECLARE_FEATURE(kGlicSetWebContentsVisibilityWhenToggling);

BASE_DECLARE_FEATURE(kGlicAnchorEntryPointForOnboardedUsers);
BASE_DECLARE_FEATURE(kGlicProcessCounterAbuseVerdict);
BASE_DECLARE_FEATURE(kGlicNoWebUiLoader);
BASE_DECLARE_FEATURE(kGlicGeminiEnterpriseSettingsEnabled);

}  // namespace features

#endif  // CHROME_BROWSER_GLIC_PUBLIC_FEATURES_H_
