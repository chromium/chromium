// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_FEATURES_SUPERVISED_USER_FEATURES_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_FEATURES_SUPERVISED_USER_FEATURES_H_

#include "base/feature_list.h"

namespace supervised_users {

BASE_DECLARE_FEATURE(kWebFilterInterstitialRefresh);

BASE_DECLARE_FEATURE(kLocalWebApprovals);
extern const char kLocalWebApprovalsPreferredButtonLocal[];
extern const char kLocalWebApprovalsPreferredButtonRemote[];

BASE_DECLARE_FEATURE(kAllowHistoryDeletionForChildAccounts);

// Returns whether refreshed version of the website filter interstitial is
// enabled.
bool IsWebFilterInterstitialRefreshEnabled();

// Returns whether local parent approvals on Family Link user's device are
// enabled.
// Local web approvals are only available when refreshed version of web
// filter interstitial is enabled.
bool IsLocalWebApprovalsEnabled();

// Returns whether the local parent approval should be displayed as the
// preferred option.
// This should only be called if IsLocalWebApprovalsEnabled() returns true.
bool IsLocalWebApprovalThePreferredButton();

// Returns whether to use the new Api for fetching.
bool IsKidsManagementServiceEnabled();

}  // namespace supervised_users

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_FEATURES_SUPERVISED_USER_FEATURES_H_
