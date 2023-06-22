// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/enterprise_util.h"

#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace base {

TEST(EnterpriseUtilMacTest, IsDeviceRegisteredWithManagementSmokeTest) {
  MacDeviceManagementState state = IsDeviceRegisteredWithManagement();

  EXPECT_NE(MacDeviceManagementState::kFailureAPIUnavailable, state);
  EXPECT_NE(MacDeviceManagementState::kFailureUnableToParseResult, state);
}

}  // namespace base
