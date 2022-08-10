// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_features.h"
#include "base/feature_list.h"

#if BUILDFLAG(IS_ANDROID)
// Enables the FamilyLink feedback collection in Chrome Settings feedback tool.
const base::Feature kEnableFamilyInfoFeedback{"EnableFamilyInfoFeedback",
                                              base::FEATURE_ENABLED_BY_DEFAULT};
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
// Enables the new style, "For You" First Run Experience
const base::Feature kForYouFre{"ForYouFre", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Enables the client-side processing of the HTTP response header
// Google-Accounts-RemoveLocalAccount.
const base::Feature kProcessGaiaRemoveLocalAccountHeader{
    "ProcessGaiaRemoveLocalAccountHeader", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the sync promo after the sign-in intercept.
const base::Feature kSyncPromoAfterSigninIntercept{
    "SyncPromoAfterSigninIntercept", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables using new style (strings, illustration, and disclaimer if needed)
// for the sign-in intercept bubble.
const base::Feature kSigninInterceptBubbleV2{"SigninInterceptBubbleV2",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Enables showing the enterprise dialog after every signin into a managed
// account.
const base::Feature kShowEnterpriseDialogForAllManagedAccountsSignin{
    "ShowEnterpriseDialogForAllManagedAccountsSignin",
    base::FEATURE_DISABLED_BY_DEFAULT};
