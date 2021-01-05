// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/util/affiliation.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace chrome {
namespace enterprise_util {

namespace {
const char kAffiliationID1[] = "id1";
const char kAffiliationID2[] = "id2";
}  // namespace

TEST(BrowserAffiliationTest, Affiliated) {
  enterprise_management::PolicyData profile_policy;
  profile_policy.add_user_affiliation_ids(kAffiliationID1);
  enterprise_management::PolicyData browser_policy;
  browser_policy.add_device_affiliation_ids(kAffiliationID1);

  EXPECT_TRUE(IsProfileAffiliated(profile_policy, browser_policy));
}

TEST(BrowserAffiliationTest, Unaffiliated) {
  enterprise_management::PolicyData profile_policy;
  profile_policy.add_user_affiliation_ids(kAffiliationID1);
  enterprise_management::PolicyData browser_policy;
  browser_policy.add_device_affiliation_ids(kAffiliationID2);

  EXPECT_FALSE(IsProfileAffiliated(profile_policy, browser_policy));
}

TEST(BrowserAffiliationTest, BrowserPolicyEmpty) {
  enterprise_management::PolicyData profile_policy;
  profile_policy.add_user_affiliation_ids(kAffiliationID1);
  enterprise_management::PolicyData browser_policy;

  EXPECT_FALSE(IsProfileAffiliated(profile_policy, browser_policy));
}

TEST(BrowserAffiliationTest, ProfilePolicyEmpty) {
  enterprise_management::PolicyData profile_policy;
  enterprise_management::PolicyData browser_policy;
  browser_policy.add_device_affiliation_ids(kAffiliationID2);

  EXPECT_FALSE(IsProfileAffiliated(profile_policy, browser_policy));
}

}  // namespace enterprise_util
}  // namespace chrome
