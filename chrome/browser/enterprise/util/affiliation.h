// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_UTIL_AFFILIATION_H_
#define CHROME_BROWSER_ENTERPRISE_UTIL_AFFILIATION_H_

#include "chrome/browser/policy/profile_policy_connector.h"

class Profile;

namespace enterprise_util {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ProfileUnaffiliatedReason {
  // The user is not managed.
  kUserUnmanaged = 0,
  // The user is managed but device is not.
  kUserByCloudAndDeviceUnmanaged = 1,
  // The user is managed but device is managed by a platform source which
  // doesn't have domain information.
  kUserByCloudAndDeviceByPlatform = 2,
  // The user is managed by a domain that is different than the device owner.
  kUserAndDeviceByCloudUnaffiliated = 3,
  kMaxValue = kUserAndDeviceByCloudUnaffiliated,
};

// Returns true if the profile and browser are managed by the same customer
// (affiliated). This is determined by comparing affiliation IDs obtained in the
// policy fetching response. If either policies has no affiliation IDs, this
// function returns false.
bool IsProfileAffiliated(Profile* profile);

// Returns more details for unaffiliated profile. Only used for unaffiliated
// profile.
ProfileUnaffiliatedReason GetUnaffiliatedReason(Profile* profile);
ProfileUnaffiliatedReason GetUnaffiliatedReason(
    policy::ProfilePolicyConnector* connector);

}  // namespace enterprise_util

#endif  // CHROME_BROWSER_ENTERPRISE_UTIL_AFFILIATION_H_
