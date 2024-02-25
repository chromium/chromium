// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TPCD_EXPERIMENT_TPCD_EXPERIMENT_FEATURES_H_
#define CHROME_BROWSER_TPCD_EXPERIMENT_TPCD_EXPERIMENT_FEATURES_H_

#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/buildflag.h"

namespace tpcd::experiment {

extern const char kVersionName[];
extern const char kDisable3PCookiesName[];
extern const char kDecisionDelayTimeName[];
extern const char kForceEligibleForTestingName[];
extern const char kNeedOnboardingForSyntheticTrialName[];
extern const char kNeedOnboardingForLabelName[];
extern const char kEnableSilentOnboardingName[];
extern const char kExclude3PCBlockedName[];
extern const char kExcludeNotSeenAdsAPIsNoticeName[];
extern const char kExcludeDasherAccountName[];
extern const char kExcludeNewUserName[];
extern const char kInstallTimeForNewUserName[];
#if BUILDFLAG(IS_ANDROID)
extern const char kExcludePwaOrTwaInstalledName[];
#endif

extern const base::FeatureParam<int> kVersion;
extern const base::FeatureParam<bool> kDisable3PCookies;
extern const base::FeatureParam<base::TimeDelta> kDecisionDelayTime;
extern const base::FeatureParam<bool> kForceEligibleForTesting;
extern const base::FeatureParam<bool> kNeedOnboardingForSyntheticTrial;
extern const base::FeatureParam<bool> kNeedOnboardingForLabel;
extern const base::FeatureParam<bool> kEnableSilentOnboarding;

// Whether to exclude users who have 3P cookies blocked.
extern const base::FeatureParam<bool> kExclude3PCBlocked;

// Whether to exclude users who have not seen the Ads APIs notice.
extern const base::FeatureParam<bool> kExcludeNotSeenAdsAPIsNotice;

// Whether to exclude Dasher accounts.
extern const base::FeatureParam<bool> kExcludeDasherAccount;

// Whether to exclude new users, i.e. their client was installed more than
// kInstallTimeForNewUser days ago.
extern const base::FeatureParam<bool> kExcludeNewUser;
// How long (number of days) a user's client needs to be installed to be
// considered a new user for exclusion.
extern const base::FeatureParam<base::TimeDelta> kInstallTimeForNewUser;

#if BUILDFLAG(IS_ANDROID)
// Whether to exclude Android users who have a PWA or TWA installed.
extern const base::FeatureParam<bool> kExcludePwaOrTwaInstalled;
#endif

extern const char kTpcdWritePopupCurrentInteractionHeuristicsGrantsName[];
extern const char kTpcdWritePopupPastInteractionHeuristicsGrantsName[];
extern const char kTpcdBackfillPopupHeuristicsGrantsName[];
extern const char kTpcdPopupHeuristicDisableForAdTaggedPopupsName[];
extern const char kTpcdPopupHeuristicEnableForIframeInitiatorName[];
extern const char kTpcdWriteRedirectHeuristicGrantsName[];
extern const char kTpcdRedirectHeuristicRequireABAFlowName[];
extern const char kTpcdRedirectHeuristicRequireCurrentInteractionName[];

// The duration of the storage access grant created when observing the Popup
// With Current Interaction scenario. If set to zero duration, do not create a
// grant.
extern const base::FeatureParam<base::TimeDelta>
    kTpcdWritePopupCurrentInteractionHeuristicsGrants;

// The duration of the storage access grant created when observing the Popup
// With Past Interaction scenario. If set to zero duration, do not create a
// grant.
extern const base::FeatureParam<base::TimeDelta>
    kTpcdWritePopupPastInteractionHeuristicsGrants;

// The lookback and duration of the storage access grants created when
// backfilling the Popup With Current Interaction scenario on onboarding to
// 3PCD. If set to zero duration, to not create backfill grants.
extern const base::FeatureParam<base::TimeDelta>
    kTpcdBackfillPopupHeuristicsGrants;

// Whether to disable writing Popup heuristic grants when the popup is opened
// via an ad-tagged frame.
extern const base::FeatureParam<bool>
    kTpcdPopupHeuristicDisableForAdTaggedPopups;

enum class EnableForIframeTypes { kNone = 0, kFirstParty = 1, kAll = 2 };

// Whether to enable writing Popup heuristic grants when the popup is opened via
// an iframe initiator.

// * kNone: Ignore popups initiated from iframes.
// * kFirstPartyIframes: Only write grants for popups initiated from 1P iframes,
// or nested tree of all 1P iframes.
// * kAllIframes: Write grants for popups initiated from any frame.
constexpr base::FeatureParam<EnableForIframeTypes>::Option
    kEnableForIframeTypesOptions[] = {
        {EnableForIframeTypes::kNone, "none"},
        {EnableForIframeTypes::kFirstParty, "first-party"},
        {EnableForIframeTypes::kAll, "all"},
};
extern const base::FeatureParam<EnableForIframeTypes>
    kTpcdPopupHeuristicEnableForIframeInitiator;

// The duration of the storage access grant created when observing the Redirect
// With Current Interaction scenario. If set to zero duration, do not create a
// grant.
extern const base::FeatureParam<base::TimeDelta>
    kTpcdWriteRedirectHeuristicGrants;

// Whether to require an A-B-A flow (where the first party preceded the
// third-party redirect in the tab history) when applying the Redirect
// heuristic.
extern const base::FeatureParam<bool> kTpcdRedirectHeuristicRequireABAFlow;

// Whether to require the third-party interaction to be in the current
// navigation when applying the Redirect heuristic.
extern const base::FeatureParam<bool>
    kTpcdRedirectHeuristicRequireCurrentInteraction;

}  // namespace tpcd::experiment

#endif  // CHROME_BROWSER_TPCD_EXPERIMENT_TPCD_EXPERIMENT_FEATURES_H_
