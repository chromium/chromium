// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_FEATURES_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/branding_buildflags.h"
#include "components/signin/public/base/signin_buildflags.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kForYouFre);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
extern const base::FeatureParam<bool> kForYouFreCloseShouldProceed;

enum class SigninPromoVariant { kSignIn, kMakeYourOwn, kDoMore };
extern const base::FeatureParam<SigninPromoVariant>
    kForYouFreSignInPromoVariant;

enum class WithDefaultBrowserStep {
  // The default browser step should be shown as appropriate.
  kYes,
  // The default browser step should be skipped.
  kNo,
  // The default browser step should be shown even if we normally should skip
  // it, example because of policies or the current default state.
  kForced,
};
extern const base::FeatureParam<WithDefaultBrowserStep>
    kForYouFreWithDefaultBrowserStep;

enum class DefaultBrowserVariant {
  // Use the current strings for the default browser prompt.
  kCurrent,
  // Use the new strings for the default browser prompt.
  kNew,
};
extern const base::FeatureParam<DefaultBrowserVariant>
    kForYouFreDefaultBrowserVariant;

BASE_DECLARE_FEATURE(kForYouFreSyntheticTrialRegistration);

extern const base::FeatureParam<std::string> kForYouFreStudyGroup;
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)

BASE_DECLARE_FEATURE(kProcessGaiaRemoveLocalAccountHeader);

BASE_DECLARE_FEATURE(kShowEnterpriseDialogForAllManagedAccountsSignin);

BASE_DECLARE_FEATURE(kDisallowManagedProfileSignout);

#if BUILDFLAG(ENABLE_MIRROR)
BASE_DECLARE_FEATURE(kVerifyRequestInitiatorForMirrorHeaders);
#endif  // BUILDFLAG(ENABLE_MIRROR)

BASE_DECLARE_FEATURE(kProfilesReordering);

BASE_DECLARE_FEATURE(kForceSigninFlowInProfilePicker);

// Used in Profile Picker signin in flow. Do not show outbound links that will
// lead to opening a browser by displaying the embedded version of the page.
BASE_DECLARE_FEATURE(kGaiaSigninUrlEmbedded);

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_FEATURES_H_
