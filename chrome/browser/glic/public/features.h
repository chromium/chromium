// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_FEATURES_H_
#define CHROME_BROWSER_GLIC_PUBLIC_FEATURES_H_

#include <string>

#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace features {

BASE_DECLARE_FEATURE(kGlicAndroidSidePanel);
BASE_DECLARE_FEATURE(kGlicDragAndDropFileUploadAndroid);

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
extern const base::FeatureParam<bool> kGlicSelectionShowCopyButtons;
extern const base::FeatureParam<bool> kGlicSelectionAutoSendPrompt;
extern const base::FeatureParam<std::string> kGlicSelectionPromptCta;
inline constexpr char kGlicSelectionPromptCtaTellMe[] = "tell_me_about_this";
inline constexpr char kGlicSelectionPromptCtaExplain[] = "explain";
extern const base::FeatureParam<bool> kGlicSelectionPromptInlineFulfillment;
extern const base::FeatureParam<std::string>
    kGlicSelectionPromptInlinePromptTemplate;
extern const base::FeatureParam<bool> kGlicSelectionPromptSkills;
extern const base::FeatureParam<std::string> kGlicSelectionDefaultBlockedSites;
base::flat_set<std::string> GetGlicSelectionDefaultBlockedSites();

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

BASE_DECLARE_FEATURE(kGlicContextMenuBelowSearch);

BASE_DECLARE_FEATURE(kGlicTextSelectionContextMenu);
extern const base::FeatureParam<bool>
    kGlicTextSelectionContextMenuMessageFirstFre;

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

BASE_DECLARE_FEATURE(kGlicGetTabFaviconById);

BASE_DECLARE_FEATURE(kGlicSkipCookieSyncOnOpen);
BASE_DECLARE_FEATURE(kGlicCookieSyncOnTokenChange);
extern const base::FeatureParam<base::TimeDelta>
    kGlicCookieSyncOnTokenChangeDelay;
extern const base::FeatureParam<bool>
    kGlicCookieSyncOnTokenChangeOnlyWhenFreCompleted;
BASE_DECLARE_FEATURE(kGlicCookieSyncOnError);
extern const base::FeatureParam<base::TimeDelta>
    kGlicCookieSyncOnErrorMinInterval;
BASE_DECLARE_FEATURE(kGlicCookieSyncOnOpenEvenIfNoSyncNeeded);

BASE_DECLARE_FEATURE(kGlicWebClientLoadTimes);
extern const base::FeatureParam<int> kGlicPreLoadingTimeMs;
extern const base::FeatureParam<int> kGlicMinLoadingTimeMs;
extern const base::FeatureParam<int> kGlicMaxLoadingTimeMs;
extern const base::FeatureParam<int> kGlicReloadMaxLoadingTimeMs;

BASE_DECLARE_FEATURE(kGlicContextualCueingV2AutoSubmit);
BASE_DECLARE_FEATURE(kGlicContextualCueV2ActiveUserBackoff);
extern const base::FeatureParam<int> kMinDaysSinceLastInvocation;

BASE_DECLARE_FEATURE(kGlicMessageFirstFreForContextualCue);

BASE_DECLARE_FEATURE(kGlicWebDragAndDropFileUpload);

BASE_DECLARE_FEATURE(kGlicOptInImpressionMetrics);

BASE_DECLARE_FEATURE(kGlicContentsInitiallyHidden);
BASE_DECLARE_FEATURE(kGlicShowForSignedOut);

BASE_DECLARE_FEATURE(kGlicAnchorEntryPointForOnboardedUsers);
BASE_DECLARE_FEATURE(kGlicProcessCounterAbuseVerdict);
BASE_DECLARE_FEATURE(kGlicNoWebUiLoader);
BASE_DECLARE_FEATURE(kGlicGeminiEnterpriseSettingsEnabled);
BASE_DECLARE_FEATURE(kGlicGeminiEnterpriseConsentEnabled);

BASE_DECLARE_FEATURE(kGlicMarketingAutoOpen);
extern const base::FeatureParam<std::string> kGlicMarketingUrlAllowlist;
extern const base::FeatureParam<int> kGlicMarketingAutoOpenMaxCount;

BASE_DECLARE_FEATURE(kGlicHotkeyLocalScope);

BASE_DECLARE_FEATURE(kGlicPasteEligibilityCheck);
BASE_DECLARE_FEATURE(kGlicWebPasteEligibilityCheck);

BASE_DECLARE_FEATURE(kGlicTabGroups);
extern const base::FeatureParam<bool> kGlicTabGroupsUseFullTabEmbedder;
BASE_DECLARE_FEATURE(kGlicSparkSettingsAccessibleLabels);

BASE_DECLARE_FEATURE(kGlicOptInDialogA11yFix);
BASE_DECLARE_FEATURE(kGlicStructuredYieldMetadata);


BASE_DECLARE_FEATURE(kGlicNoWebview);
BASE_DECLARE_FEATURE(kGlicDisconnectedWebview);

BASE_DECLARE_FEATURE(kGlicShakeTrigger);

BASE_DECLARE_FEATURE(kGlicAndroidTablet);

BASE_DECLARE_FEATURE(kGlicActionFirstFRE);

BASE_DECLARE_FEATURE(kGlicWarmOnNudge);

BASE_DECLARE_FEATURE(kGlicWarmOnIph);
}  // namespace features

#endif  // CHROME_BROWSER_GLIC_PUBLIC_FEATURES_H_
