// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_FEATURES_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "components/signin/public/base/signin_buildflags.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kForYouFre);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
extern const base::FeatureParam<bool> kForYouFreCloseShouldProceed;

enum class SigninPromoVariant { kSignIn, kMakeYourOwn, kDoMore };
extern const base::FeatureParam<SigninPromoVariant>
    kForYouFreSignInPromoVariant;

BASE_DECLARE_FEATURE(kForYouFreSyntheticTrialRegistration);

extern const base::FeatureParam<std::string> kForYouFreStudyGroup;
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)

BASE_DECLARE_FEATURE(kProcessGaiaRemoveLocalAccountHeader);

BASE_DECLARE_FEATURE(kSyncPromoAfterSigninIntercept);

BASE_DECLARE_FEATURE(kSigninInterceptBubbleV2);

BASE_DECLARE_FEATURE(kShowEnterpriseDialogForAllManagedAccountsSignin);

BASE_DECLARE_FEATURE(kDisallowManagedProfileSignout);

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_FEATURES_H_
