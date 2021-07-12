// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_features.h"

// Enables the client-side processing of the HTTP response header
// Google-Accounts-RemoveLocalAccount.
const base::Feature kProcessGaiaRemoveLocalAccountHeader{
    "ProcessGaiaRemoveLocalAccountHeader", base::FEATURE_ENABLED_BY_DEFAULT};

// Allows policies to be loaded on a managed account without activating sync.
// Uses enterprise confirmation dialog for managed accounts signin outside of
// the profile picker.
extern const base::Feature kAccountPoliciesLoadedWithoutSync{
    "AccountPoliciesLoadedWithoutSync", base::FEATURE_DISABLED_BY_DEFAULT};
