// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/enterprise_util.h"

#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace base {

TEST(EnterpriseUtilMacTest, IsDeviceRegisteredWithManagementOldSmokeTest) {
  MacDeviceManagementStateOld state = IsDeviceRegisteredWithManagementOld();

  EXPECT_NE(MacDeviceManagementStateOld::kFailureAPIUnavailable, state);
  EXPECT_NE(MacDeviceManagementStateOld::kFailureUnableToParseResult, state);
}

TEST(EnterpriseUtilMacTest, IsDeviceRegisteredWithManagementNewSmokeTest) {
  MacDeviceManagementStateNew state = IsDeviceRegisteredWithManagementNew();

  if (@available(macOS 10.13.4, *)) {
    EXPECT_NE(MacDeviceManagementStateNew::kFailureAPIUnavailable, state);
    EXPECT_NE(MacDeviceManagementStateNew::kFailureUnableToParseResult, state);
  } else {
    EXPECT_EQ(MacDeviceManagementStateNew::kFailureAPIUnavailable, state);
  }
}

}  // namespace base
