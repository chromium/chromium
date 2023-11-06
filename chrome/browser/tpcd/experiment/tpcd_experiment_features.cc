// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/experiment/tpcd_experiment_features.h"

#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "content/public/common/content_features.h"

namespace tpcd::experiment {

const char kVersionName[] = "version";
const char kDisable3PCookiesName[] = "disable_3p_cookies";
const char kForceEligibleForTestingName[] = "force_eligible";
const char kDecisionDelayTimeName[] = "decision_delay_time";
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

// Set whether to force client being eligible for manual testing.
const base::FeatureParam<bool> kForceEligibleForTesting{
    &features::kCookieDeprecationFacilitatedTesting,
    /*name=*/kForceEligibleForTestingName,
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

}  // namespace tpcd::experiment
