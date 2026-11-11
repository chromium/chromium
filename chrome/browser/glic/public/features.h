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

BASE_DECLARE_FEATURE(kGlicChromeStatusIconWin);

BASE_DECLARE_FEATURE(kGlicOSIconVariant);
extern const base::FeatureParam<int> kGlicOSIconVariantParam;

BASE_DECLARE_FEATURE(kGlicOrphanedReattachment);

BASE_DECLARE_FEATURE(kGlicInvoke);

BASE_DECLARE_FEATURE(kGlicSelectionOverlayPrompt);
BASE_DECLARE_FEATURE(kGlicSelectionOverlayPromptBox);
BASE_DECLARE_FEATURE(kGlicSelectionOverlayDogfoodFeedback);
BASE_DECLARE_FEATURE_PARAM(std::string,
                           kGlicSelectionOverlayDogfoodFeedbackUrl);
BASE_DECLARE_FEATURE(kGlicSelectionSuggestions);
BASE_DECLARE_FEATURE_PARAM(std::string, kGlicSelectionSuggestionsCapabilities);
BASE_DECLARE_FEATURE(kQuickAnswersSelectionSuggestions);

BASE_DECLARE_FEATURE(kGlicSelectionSmallChip);
extern const base::FeatureParam<bool> kGlicSelectionSmallChipOnTop;

BASE_DECLARE_FEATURE(kGlicSuggestionContextMenu);
BASE_DECLARE_FEATURE(kGlicSmartSelectionToolbarButton);

BASE_DECLARE_FEATURE(kGlicCreateTabAdjacent);

BASE_DECLARE_FEATURE(kGlicLiveMode);

BASE_DECLARE_FEATURE(kGlicSummarizeVideoSuggestion);

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

// Exempts the user from ActorPolicyChecker.
BASE_DECLARE_FEATURE(kGlicActorPolicyControlExemption);

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
BASE_DECLARE_FEATURE(kGlicReuseCookies);

BASE_DECLARE_FEATURE(kGlicWebClientLoadTimes);
extern const base::FeatureParam<int> kGlicPreLoadingTimeMs;
extern const base::FeatureParam<int> kGlicMinLoadingTimeMs;
extern const base::FeatureParam<int> kGlicMaxLoadingTimeMs;

BASE_DECLARE_FEATURE(kGlicContextualCueingV2AutoSubmit);
BASE_DECLARE_FEATURE(kGlicContextualCuesHandleEdu);
BASE_DECLARE_FEATURE(kGlicContextualCuesHandleShopping);
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
BASE_DECLARE_FEATURE(kGlicNativeLoadingPlaceholder);
bool IsGlicNativeLoadingPlaceholderEnabled();

// TODO(b/576288240): Move the GEiC features below into a GEiC-specific
// header and namespace under chrome/browser/glic/gemini_enterprise/.

// Enables Gemini Enterprise in Chrome (GEiC), the standalone enterprise
// surface.
BASE_DECLARE_FEATURE(kGeic);
// Allows the GEiC surface to be turned off from within the kGeic study without
// having to disable the study itself.
extern const base::FeatureParam<bool> kGeicEnabledParam;
// The Gemini Enterprise guest URL. Intentionally empty by default: running in
// GEiC mode without an explicit GEiC URL is a misconfiguration, so callers
// error rather than fall back to the consumer Gemini URL.
extern const base::FeatureParam<std::string> kGeicGuestURL;
// Enables the Gemini Enterprise in Chrome (GEiC) first-run experience (FRE).
// When enabled for a GEiC profile, Chrome stores FRE completion per GE instance
// in `geic.completed_fre`, exposes it to the GE web app, and gates GEiC
// features on its completion. Without it, GEiC profiles are always consented.
BASE_DECLARE_FEATURE(kGeicFre);

// Allows the Gemini Enterprise (GEiC) guest to use the Glic actor (web
// actuation).
BASE_DECLARE_FEATURE(kGeicActuation);

// Lets the Gemini Enterprise (GEiC) guest use server-side rendering. This is
// independent of `kGlicSsr`, which only applies to consumer Gemini, so the two
// can be rolled out separately.
BASE_DECLARE_FEATURE(kGeicSsr);

// Prompts the user to relaunch Chrome when the Glic provider configuration
// (e.g. the `GeminiEnterpriseUrl` policy) changes mid-session. Requires
// `kGeic`.
BASE_DECLARE_FEATURE(kGeicProviderChangePrompt);

BASE_DECLARE_FEATURE(kGlicHotkeyLocalScope);

BASE_DECLARE_FEATURE(kGlicPasteEligibilityCheck);
BASE_DECLARE_FEATURE(kGlicWebPasteEligibilityCheck);

BASE_DECLARE_FEATURE(kGlicTabGroups);
BASE_DECLARE_FEATURE(kGlicSparkSettingsAccessibleLabels);

BASE_DECLARE_FEATURE(kGlicOptInDialogA11yFix);
BASE_DECLARE_FEATURE(kGlicStructuredYieldMetadata);

BASE_DECLARE_FEATURE(kGlicNoWebview);
// When enabled alongside GlicNoWebview, disables the High Frequency Cookie
// Rotation (HFCR) frame in Glic. See b/565392099.
BASE_DECLARE_FEATURE(kGlicDisableHfcr);
// When enabled, appends the primary account email as the authuser query
// parameter to the guest URL when NoWebview is enabled.
BASE_DECLARE_FEATURE(kGlicSetAuthUser);
BASE_DECLARE_FEATURE(kGlicDisconnectedWebview);
BASE_DECLARE_FEATURE(kGlicGuestInitialSize);

BASE_DECLARE_FEATURE(kGlicShakeTrigger);
extern const base::FeatureParam<bool> kGlicShakeTriggerOnlyOnSidePanel;

BASE_DECLARE_FEATURE(kGlicAndroidTablet);

// Enables voice input for Gemini in Chrome, including the microphone permission
// toggle in Glic settings.
BASE_DECLARE_FEATURE(kGlicVoice);

// When enabled, Auto Browse settings show a variant of the second "things to
// consider" disclosure to users in the countries listed by its
// `enabled_countries` param (defaults to "gb,au,nz").
BASE_DECLARE_FEATURE(kGlicWebActuationDisclosureCountryVariant);

// When enabled, the Android experimental opt-in dialog uses a taller 380x710dp
// max size, so its content fits without scrolling, instead of the default
// 380x567dp.
BASE_DECLARE_FEATURE(kGlicExperimentalOptInDialogNonScrollable);

#if BUILDFLAG(IS_ANDROID)
// Enables the native FRE on Android.
BASE_DECLARE_FEATURE(kGlicNativeFreAndroid);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_DECLARE_FEATURE(kGlicActionFirstFRE);

BASE_DECLARE_FEATURE(kGlicWarmOnNudge);

BASE_DECLARE_FEATURE(kGlicWarmOnIph);

BASE_DECLARE_FEATURE(kGlicWarmOnGeminiWeb);

BASE_DECLARE_FEATURE(kGlicBackfillWarmingUsePerformanceManager);

BASE_DECLARE_FEATURE(kGlicColdWarmingUsePerformanceManager);

BASE_DECLARE_FEATURE(kGlicSsr);
extern const base::FeatureParam<bool> kGlicShowLoaderUntilMainPageOnload;

BASE_DECLARE_FEATURE(kGlicClientDataStorage);

BASE_DECLARE_FEATURE(kGlicShowNotTellTutorials);
extern const base::FeatureParam<int> kGlicShowNotTellTutorialsArm;

BASE_DECLARE_FEATURE(kGlicActivityHub);

// Uses the updated copy and title layout for the Glic experimental triggering
// OS notification. Kill switch: when disabled, the original strings are shown.
BASE_DECLARE_FEATURE(kGlicExperimentalTriggeringOsNotificationUpdatedStrings);

BASE_DECLARE_FEATURE(kGlicRecordPeriodicInstanceCountMetrics);

BASE_DECLARE_FEATURE(kGlicWhatsNewLuminous);
BASE_DECLARE_FEATURE(kGlicBrowserCommandFallback);
BASE_DECLARE_FEATURE_PARAM(std::string, kGlicBrowserCommandFallbackUrl);

// Test only. When enabled, Android drops incoming Glic experimental triggering
// TriggerActuationRequest messages without replying or acking, as if the FCM
// message never arrived.
BASE_DECLARE_FEATURE(kGlicForceFcmExperimentalTriggerFailure);

// Allows experimental triggering actuation sessions to wait for a reloaded
// guest web client to reconnect and re-deliver the prompt when the guest
// disconnects before sending its first response (b/581951225).
BASE_DECLARE_FEATURE(kGlicExperimentalTriggeringNavigationReconnectGrace);
extern const base::FeatureParam<base::TimeDelta>
    kGlicExperimentalTriggeringNavigationReconnectGracePeriod;

// Highlights the Ask Gemini button when users who started from a secondary
// entry point close the Glic panel.
BASE_DECLARE_FEATURE(kGlicAskGeminiWayfinding);

// Keeps a GlicInstance alive across side-panel embedder unbinds and blank
// instance cleanup while an experimental triggering actuation session is
// active, unless the last bound/pinned tab itself was closed (b/581951225).
BASE_DECLARE_FEATURE(kGlicExperimentalTriggeringKeepInstanceOnUnbind);

BASE_DECLARE_FEATURE(kGlicConversationSharing);
extern const base::FeatureParam<std::string>
    kGlicConversationSharingCaptureUrls;

#if BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kGlicExperimentalTriggeringBackgroundOptInNotification);
extern const base::FeatureParam<base::TimeDelta>
    kGlicExperimentalTriggeringBackgroundOptInTimeout;
#endif  // BUILDFLAG(IS_ANDROID)
}  // namespace features

#endif  // CHROME_BROWSER_GLIC_PUBLIC_FEATURES_H_
