// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_FEATURES_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_FEATURES_H_

#include "base/feature_list.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kForYouFre);
#endif

BASE_DECLARE_FEATURE(kDelayConsentLevelUpgrade);

BASE_DECLARE_FEATURE(kProcessGaiaRemoveLocalAccountHeader);

BASE_DECLARE_FEATURE(kSyncPromoAfterSigninIntercept);

BASE_DECLARE_FEATURE(kSigninInterceptBubbleV2);

BASE_DECLARE_FEATURE(kShowEnterpriseDialogForAllManagedAccountsSignin);

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_FEATURES_H_
