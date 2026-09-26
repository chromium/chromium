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

// Macros to define the default state of features explicitly across platforms.
#if BUILDFLAG(IS_ANDROID)
#define FEATURE_ENABLED_BY_DEFAULT_NON_ANDROID base::FEATURE_DISABLED_BY_DEFAULT
#define FEATURE_ENABLED_BY_DEFAULT_ANDROID_ONLY base::FEATURE_ENABLED_BY_DEFAULT
#else
#define FEATURE_ENABLED_BY_DEFAULT_NON_ANDROID base::FEATURE_ENABLED_BY_DEFAULT
#define FEATURE_ENABLED_BY_DEFAULT_ANDROID_ONLY \
  base::FEATURE_DISABLED_BY_DEFAULT
#endif

#define FEATURE_ENABLED_BY_DEFAULT_ALL_PLATFORMS \
  base::FEATURE_ENABLED_BY_DEFAULT

namespace features {

BASE_DECLARE_FEATURE(kGlicAndroidSidePanel);
BASE_DECLARE_FEATURE(kGlicDragAndDropFileUploadAndroid);

// Enables attaching the Glic WebUI WebContents to the offscreen rendering
// manager on Android, so it keeps executing JavaScript, servicing Mojo IPC and
// scheduling frames while Chrome is not in the foreground.
BASE_DECLARE_FEATURE(kGlicAndroidOffscreenRendering);

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

// Controls inline cue for text selection.
BASE_DECLARE_FEATURE(kGlicSelectionPrompt);
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

BASE_DECLARE_FEATURE(kGlicSelectionOverlayPrompt);

BASE_DECLARE_FEATURE(kGlicSelectionSmallChip);
extern const base::FeatureParam<bool> kGlicSelectionSmallChipOnTop;

BASE_DECLARE_FEATURE(kGlicCreateTabAdjacent);

BASE_DECLARE_FEATURE(kGlicDynamicChromeTools);

BASE_DECLARE_FEATURE(kGlicLiveMode);

BASE_DECLARE_FEATURE(kGlicSummarizeVideoSuggestion);

BASE_DECLARE_FEATURE(kGlicFixTimeToFirstQueryKillSwitch);

BASE_DECLARE_FEATURE(kGlicContextMenu);
extern const base::FeatureParam<std::string> kGlicContextMenuArm;
extern const base::FeatureParam<bool> kGlicContextMenuWithOnboarding;

BASE_DECLARE_FEATURE(kGlicContextMenuBelowSearch);

BASE_DECLARE_FEATURE(kGlicTextSelectionContextMenu);
extern const base::FeatureParam<bool>
    kGlicTextSelectionContextMenuMessageFirstFre;
// Whether the selected text is auto-submitted with a default prompt instead of
// only being attached to the input area for the user to submit manually.
extern const base::FeatureParam<bool> kGlicTextSelectionContextMenuAutoSubmit;

BASE_DECLARE_FEATURE(kGlicTieredRolloutV2);
extern const base::FeatureParam<std::string> kGlicTieredRolloutV2EligibleTiers;
const base::flat_set<int32_t>& GetGlicTieredRolloutV2EligibleTiers();

// When enabled, Glic and Autobrowse (web actuation) entitlement is determined
// by the synced subscription benefits priority pref
// (`subscription_eligibility::prefs::kSubscriptionBenefits`) instead of the AI
// subscription tier (`kAiSubscriptionTier`).
BASE_DECLARE_FEATURE(kGlicSubscriptionBenefitsEligibility);
// Comma separated list of subscription benefit values which make a profile
// eligible for Glic.
extern const base::FeatureParam<std::string> kGlicEligibleBenefits;
// Comma separated list of subscription benefit values which make a profile
// eligible for Autobrowse (web actuation).
extern const base::FeatureParam<std::string> kGlicActorEligibleBenefits;
// These parse the params on each call, so that experiment configurations
// applied after startup (e.g. in tests) are respected.
base::flat_set<std::string> GetGlicEligibleBenefits();
base::flat_set<std::string> GetGlicActorEligibleBenefits();

// Returns true if `profile_benefits`, the benefits stored in the subscription
// benefits priority pref, contains at least one of `eligible_benefits`.
bool HasAnyEligibleGlicBenefit(
    const base::flat_set<std::string>& profile_benefits,
    const base::flat_set<std::string>& eligible_benefits);

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
BASE_DECLARE_FEATURE(kGlicCookieSyncEarlyNoStartup);

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

// Enables Gemini Enterprise in Chrome (GEiC), the standalone enterprise
// surface. This is distinct from Gemini Enterprise as a Tool (GEaaT), which is
// governed by kGlicGeminiEnterpriseSettingsEnabled above.
BASE_DECLARE_FEATURE(kGeic);
// Allows the GEiC surface to be turned off from within the kGeic study without
// having to disable the study itself.
extern const base::FeatureParam<bool> kGeicEnabledParam;
// The Gemini Enterprise guest URL. Intentionally empty by default: running in
// GEiC mode without an explicit GEiC URL is a misconfiguration, so callers
// error rather than fall back to the consumer Gemini URL.
extern const base::FeatureParam<std::string> kGeicGuestURL;

BASE_DECLARE_FEATURE(kGlicMarketingAutoOpen);
extern const base::FeatureParam<std::string> kGlicMarketingUrlAllowlist;
extern const base::FeatureParam<int> kGlicMarketingAutoOpenMaxCount;

BASE_DECLARE_FEATURE(kGlicHotkeyLocalScope);

BASE_DECLARE_FEATURE(kGlicPasteEligibilityCheck);
BASE_DECLARE_FEATURE(kGlicWebPasteEligibilityCheck);

BASE_DECLARE_FEATURE(kGlicTabGroups);
BASE_DECLARE_FEATURE(kGlicSparkSettingsAccessibleLabels);

BASE_DECLARE_FEATURE(kGlicOptInDialogA11yFix);
BASE_DECLARE_FEATURE(kGlicStructuredYieldMetadata);

BASE_DECLARE_FEATURE(kGlicNoWebview);
// Returns true if kGlicNoWebview is enabled or if GEiC is enabled.
bool IsGlicNoWebviewEnabled();
BASE_DECLARE_FEATURE(kGlicDisconnectedWebview);

BASE_DECLARE_FEATURE(kGlicShakeTrigger);
extern const base::FeatureParam<bool> kGlicShakeTriggerOnlyOnSidePanel;

BASE_DECLARE_FEATURE(kGlicAndroidTablet);

// Enables voice input for Gemini in Chrome, including the microphone permission
// toggle in Glic settings.
BASE_DECLARE_FEATURE(kGlicVoice);

// When enabled, the Android experimental opt-in dialog uses a taller 380x710dp
// max size, so its content fits without scrolling, instead of the default
// 380x567dp.
BASE_DECLARE_FEATURE(kGlicExperimentalOptInDialogNonScrollable);

BASE_DECLARE_FEATURE(kGlicActionFirstFRE);

BASE_DECLARE_FEATURE(kGlicWarmOnNudge);

BASE_DECLARE_FEATURE(kGlicWarmOnIph);

BASE_DECLARE_FEATURE(kGlicBackfillWarmingUsePerformanceManager);

BASE_DECLARE_FEATURE(kGlicColdWarmingUsePerformanceManager);
}  // namespace features

#endif  // CHROME_BROWSER_GLIC_PUBLIC_FEATURES_H_
