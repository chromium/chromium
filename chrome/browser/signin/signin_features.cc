// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_features.h"

// Enables the client-side processing of the HTTP response header
// Google-Accounts-RemoveLocalAccount.
const base::Feature kProcessGaiaRemoveLocalAccountHeader{
    "ProcessGaiaRemoveLocalAccountHeader", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the sync promo after the sign-in intercept.
const base::Feature kSyncPromoAfterSigninIntercept{
    "SyncPromoAfterSigninIntercept", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables showing the enterprise dialog after every signin into a managed
// account.
const base::Feature kShowEnterpriseDialogForAllManagedAccountsSignin{
    "ShowEnterpriseDialogForAllManagedAccountsSignin",
    base::FEATURE_DISABLED_BY_DEFAULT};
