// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_FEATURES_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_FEATURES_H_

#include "base/feature_list.h"

#if BUILDFLAG(IS_ANDROID)
extern const base::Feature kEnableFamilyInfoFeedback;
#endif

extern const base::Feature kProcessGaiaRemoveLocalAccountHeader;

extern const base::Feature kSyncPromoAfterSigninIntercept;

extern const base::Feature kSigninInterceptBubbleV2;

extern const base::Feature kShowEnterpriseDialogForAllManagedAccountsSignin;

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_FEATURES_H_
