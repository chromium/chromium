// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_FEATURES_SUPERVISED_USER_FEATURES_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_FEATURES_SUPERVISED_USER_FEATURES_H_

#include "base/feature_list.h"

namespace supervised_users {

extern const base::Feature kWebFilterInterstitialRefresh;

extern const base::Feature kLocalWebApprovals;

extern const base::Feature kAllowHistoryDeletionForChildAccounts;

// Returns whether refreshed version of the website filter interstitial is
// enabled.
bool IsWebFilterInterstitialRefreshEnabled();

// Returns whether local parent approvals on Family Link user's device are
// enabled.
// Local web approvals are only available when refreshed version of web
// filter interstitial is enabled.
bool IsLocalWebApprovalsEnabled();

}  // namespace supervised_users

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_FEATURES_SUPERVISED_USER_FEATURES_H_
