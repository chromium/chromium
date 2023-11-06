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

}  // namespace tpcd::experiment

#endif  // CHROME_BROWSER_TPCD_EXPERIMENT_TPCD_EXPERIMENT_FEATURES_H_
