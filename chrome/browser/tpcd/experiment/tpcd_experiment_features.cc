// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/experiment/tpcd_experiment_features.h"

#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/content_settings/core/common/features.h"
#include "content/public/common/content_features.h"

namespace tpcd::experiment {

const char kVersionName[] = "version";
const char kDisable3PCookiesName[] = "disable_3p_cookies";
const char kForceEligibleForTestingName[] = "force_eligible";
const char kDecisionDelayTimeName[] = "decision_delay_time";
const char kNeedOnboardingForSyntheticTrialName[] =
    "need_onboarding_for_synthetic_trial";
const char kNeedOnboardingForLabelName[] = "need_onboarding_for_label";
const char kEnableSilentOnboardingName[] = "enable_silent_onboarding";
const char kExclude3PCBlockedName[] = "exclude_3pc_blocked";
const char kExcludeNotSeenAdsAPIsNoticeName[] = "exclude_has_not_seen_notice";
const char kExcludeDasherAccountName[] = "exclude_dasher_account";
const char kExcludeNewUserName[] = "exclude_new_user";
const char kInstallTimeForNewUserName[] = "install_time_for_new_user";
#if BUILDFLAG(IS_ANDROID)
const char kExcludePwaOrTwaInstalledName[] = "exclude_pwa_or_twa_installed";
#endif

// Set the version of the experiment finch config.
const base::FeatureParam<int> kVersion{
    &features::kCookieDeprecationFacilitatedTesting, /*name=*/kVersionName,
    /*default_value=*/0};

// True IFF third-party cookies are disabled.
// Distinguishes between "Mode A" and "Mode B" cohorts.
const base::FeatureParam<bool> kDisable3PCookies{
    &features::kCookieDeprecationFacilitatedTesting,
    /*name=*/kDisable3PCookiesName,
    /*default_value=*/false};

const base::FeatureParam<base::TimeDelta> kDecisionDelayTime{
    &features::kCookieDeprecationFacilitatedTesting,
    /*name=*/kDecisionDelayTimeName,
    /*default_value=*/base::Seconds(1)};

// Set whether to force client being eligible for manual testing. When
// "disable_3p_cookies" feature param is false, this feature param is only
// meaningful when "enable_silent_onboarding" feature param is true.
const base::FeatureParam<bool> kForceEligibleForTesting{
    &features::kCookieDeprecationFacilitatedTesting,
    /*name=*/kForceEligibleForTestingName,
    /*default_value=*/false};

// Set whether to wait for onboarding to register the synthetic trial. When
// "disable_3p_cookies" feature param is false, this feature param is only
// meaningful when "enable_silent_onboarding" feature param is true.
const base::FeatureParam<bool> kNeedOnboardingForSyntheticTrial{
    &features::kCookieDeprecationFacilitatedTesting,
    /*name=*/kNeedOnboardingForSyntheticTrialName,
    /*default_value=*/false};

// Set whether to wait for onboarding to send the label.
const base::FeatureParam<bool> kNeedOnboardingForLabel{
    &features::kCookieDeprecationFacilitatedTesting,
    /*name=*/kNeedOnboardingForLabelName,
    /*default_value=*/false};

// Set whether to enable silent onboarding. Only meaningful when
// "disable_3p_cookies" feature param is false, and should be enabled if
// either "need_onboarding_for_synthetic_trial" or "need_onboarding_for_label"
// feature param is enabled.
const base::FeatureParam<bool> kEnableSilentOnboarding{
    &features::kCookieDeprecationFacilitatedTesting,
    /*name=*/kEnableSilentOnboardingName,
    /*default_value=*/false};

const base::FeatureParam<bool> kExclude3PCBlocked{
    &features::kCookieDeprecationFacilitatedTesting,
    /*name=*/kExclude3PCBlockedName,
    /*default_value=*/true};

const base::FeatureParam<bool> kExcludeNotSeenAdsAPIsNotice{
    &features::kCookieDeprecationFacilitatedTesting,
    /*name=*/kExcludeNotSeenAdsAPIsNoticeName,
    /*default_value=*/true};

const base::FeatureParam<bool> kExcludeDasherAccount{
    &features::kCookieDeprecationFacilitatedTesting,
    /*name=*/kExcludeDasherAccountName,
    /*default_value=*/true};

const base::FeatureParam<bool> kExcludeNewUser{
    &features::kCookieDeprecationFacilitatedTesting,
    /*name=*/kExcludeNewUserName,
    /*default_value=*/true};

const base::FeatureParam<base::TimeDelta> kInstallTimeForNewUser{
    &features::kCookieDeprecationFacilitatedTesting,
    /*name=*/kInstallTimeForNewUserName,
    /*default_value=*/base::Days(30)};

#if BUILDFLAG(IS_ANDROID)
const base::FeatureParam<bool> kExcludePwaOrTwaInstalled{
    &features::kCookieDeprecationFacilitatedTesting,
    /*name=*/kExcludePwaOrTwaInstalledName,
    /*default_value=*/true};
#endif

const char kTpcdWritePopupCurrentInteractionHeuristicsGrantsName[] =
    "TpcdWritePopupCurrentInteractionHeuristicsGrants";
const char kTpcdWritePopupPastInteractionHeuristicsGrantsName[] =
    "TpcdWritePopupPastInteractionHeuristicsGrants";
const char kTpcdBackfillPopupHeuristicsGrantsName[] =
    "TpcdBackfillPopupHeuristicsGrants";
const char kTpcdPopupHeuristicDisableForAdTaggedPopupsName[] =
    "TpcdPopupHeuristicDisableForAdTaggedPopups";
const char kTpcdPopupHeuristicEnableForIframeInitiatorName[] =
    "TpcdPopupHeuristicEnableForIframeInitiator";
const char kTpcdWriteRedirectHeuristicGrantsName[] =
    "TpcdWriteRedirectHeuristicGrants";
const char kTpcdRedirectHeuristicRequireABAFlowName[] =
    "TpcdRedirectHeuristicRequireABAFlow";
const char kTpcdRedirectHeuristicRequireCurrentInteractionName[] =
    "TpcdRedirectHeuristicRequireCurrentInteraction";

const base::FeatureParam<base::TimeDelta>
    kTpcdWritePopupCurrentInteractionHeuristicsGrants{
        &content_settings::features::kTpcdHeuristicsGrants,
        kTpcdWritePopupCurrentInteractionHeuristicsGrantsName, base::Days(30)};

const base::FeatureParam<base::TimeDelta>
    kTpcdWritePopupPastInteractionHeuristicsGrants{
        &content_settings::features::kTpcdHeuristicsGrants,
        kTpcdWritePopupPastInteractionHeuristicsGrantsName, base::TimeDelta()};

const base::FeatureParam<base::TimeDelta> kTpcdBackfillPopupHeuristicsGrants{
    &content_settings::features::kTpcdHeuristicsGrants,
    kTpcdBackfillPopupHeuristicsGrantsName, base::Days(30)};

const base::FeatureParam<bool> kTpcdPopupHeuristicDisableForAdTaggedPopups{
    &content_settings::features::kTpcdHeuristicsGrants,
    kTpcdPopupHeuristicDisableForAdTaggedPopupsName, false};

const base::FeatureParam<EnableForIframeTypes>
    kTpcdPopupHeuristicEnableForIframeInitiator{
        &content_settings::features::kTpcdHeuristicsGrants,
        kTpcdPopupHeuristicEnableForIframeInitiatorName,
        EnableForIframeTypes::kAll, &kEnableForIframeTypesOptions};

const base::FeatureParam<base::TimeDelta> kTpcdWriteRedirectHeuristicGrants{
    &content_settings::features::kTpcdHeuristicsGrants,
    kTpcdWriteRedirectHeuristicGrantsName, base::Minutes(15)};

const base::FeatureParam<bool> kTpcdRedirectHeuristicRequireABAFlow{
    &content_settings::features::kTpcdHeuristicsGrants,
    kTpcdRedirectHeuristicRequireABAFlowName, true};

const base::FeatureParam<bool> kTpcdRedirectHeuristicRequireCurrentInteraction{
    &content_settings::features::kTpcdHeuristicsGrants,
    kTpcdRedirectHeuristicRequireCurrentInteractionName, true};

}  // namespace tpcd::experiment
