// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/parent_access_code/parent_access_test_utils.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace parent_access {

AccessCodeConfig GetDefaultTestConfig() {
  return AccessCodeConfig(kTestSharedSecret, kDefaultCodeValidity,
                          kDefaultClockDrift);
}

AccessCodeConfig GetInvalidTestConfig() {
  return AccessCodeConfig("AAAAaaaaBBBBbbbbccccCCCC", kDefaultCodeValidity,
                          kDefaultClockDrift);
}

void GetTestAccessCodeValues(AccessCodeValues* test_values) {
  base::Time timestamp;
  ASSERT_TRUE(base::Time::FromString("8 Jan 2019 16:58:07 PST", &timestamp));
  (*test_values)[timestamp] = "734261";
  ASSERT_TRUE(base::Time::FromString("14 Jan 2019 15:35:05 PST", &timestamp));
  (*test_values)[timestamp] = "472150";
  ASSERT_TRUE(base::Time::FromString("14 Jan 2019 15:42:49 PST", &timestamp));
  (*test_values)[timestamp] = "204984";
  ASSERT_TRUE(base::Time::FromString("14 Jan 2019 15:53:01 PST", &timestamp));
  (*test_values)[timestamp] = "157758";
  ASSERT_TRUE(base::Time::FromString("14 Jan 2019 16:00:00 PST", &timestamp));
  (*test_values)[timestamp] = "524186";
}

}  // namespace parent_access
}  // namespace chromeos
