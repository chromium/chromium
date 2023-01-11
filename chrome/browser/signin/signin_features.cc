// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_features.h"
#include "base/feature_list.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
// Enables the new style, "For You" First Run Experience
BASE_FEATURE(kForYouFre, "ForYouFre", base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Enables the client-side processing of the HTTP response header
// Google-Accounts-RemoveLocalAccount.
BASE_FEATURE(kProcessGaiaRemoveLocalAccountHeader,
             "ProcessGaiaRemoveLocalAccountHeader",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the sync promo after the sign-in intercept.
BASE_FEATURE(kSyncPromoAfterSigninIntercept,
             "SyncPromoAfterSigninIntercept",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables using new style (strings, illustration, and disclaimer if needed)
// for the sign-in intercept bubble.
BASE_FEATURE(kSigninInterceptBubbleV2,
             "SigninInterceptBubbleV2",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables showing the enterprise dialog after every signin into a managed
// account.
BASE_FEATURE(kShowEnterpriseDialogForAllManagedAccountsSignin,
             "ShowEnterpriseDialogForAllManagedAccountsSignin",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Disables signout for enteprise managed profiles
BASE_FEATURE(kDisallowManagedProfileSignout,
             "DisallowManagedProfileSignout",
             base::FEATURE_DISABLED_BY_DEFAULT);
