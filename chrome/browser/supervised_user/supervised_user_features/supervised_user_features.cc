// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_features/supervised_user_features.h"

#include "base/check.h"
#include "base/feature_list.h"

namespace supervised_users {

// Enables refreshed version of the website filter interstitial that is shown to
// Family Link users when the navigate to the blocked website.
// This feature is a prerequisite for `kLocalWebApproval` feature.
const base::Feature kWebFilterInterstitialRefresh{
    "WebFilterInterstitialRefresh", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables local parent approvals for the blocked website on the Family Link
// user's device.
// This feature requires a refreshed layout and `kWebFilterInterstitialRefresh`
// to be enabled.
const base::Feature kLocalWebApprovals{"LocalWebApprovals",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Enables child accounts (i.e. Unicorn accounts) to clear their browsing
// history data from Settings.
const base::Feature kAllowHistoryDeletionForChildAccounts{
    "AllowHistoryDeletionForChildAccounts", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsWebFilterInterstitialRefreshEnabled() {
  DCHECK(base::FeatureList::IsEnabled(kWebFilterInterstitialRefresh) ||
         !base::FeatureList::IsEnabled(kLocalWebApprovals));
  return base::FeatureList::IsEnabled(kWebFilterInterstitialRefresh);
}

bool IsLocalWebApprovalsEnabled() {
  return IsWebFilterInterstitialRefreshEnabled() &&
         base::FeatureList::IsEnabled(kLocalWebApprovals);
}

}  // namespace supervised_users
