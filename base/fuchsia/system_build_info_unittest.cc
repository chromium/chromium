// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/buildinfo/cpp/fidl.h>

#include "base/fuchsia/system_info.h"
#include "base/test/gtest_util.h"

namespace base {

// Ensures that when FetchAndCacheSystemInfo() has not been called in the
// process that a  DCHECK fires to alert the developer.
//
// TODO(crbug.com/1326674) Ensure the test passes on Fuchsia bots and
// re-enable.
TEST(BuildInfoDeathTest,
     DISABLED_GetCachedBuildInfo_DcheckIfNotAlreadyFetched) {
  // Clear the cached build info to force an error condition.
  ClearCachedSystemInfoForTesting();

  EXPECT_DCHECK_DEATH_WITH(
      { GetCachedBuildInfo(); },
      "FetchAndCacheSystemInfo\\(\\) has not been called in this process");

  // All test processes have BuildInfo cached before tests are run. Re-fetch and
  // cache the BuildInfo to restore that state for any tests that are
  // subsequently run in the same process as this one.
  EXPECT_TRUE(FetchAndCacheSystemInfo());
}

// TODO(crbug.com/1326674) Ensure the test passes on Fuchsia bots and
// re-enable.
TEST(BuildInfoTest, DISABLED_GetCachedBuildInfo_CheckExpectedValues) {
  // Ensure the cached BuildInfo is in a known state.
  ClearCachedSystemInfoForTesting();
  ASSERT_TRUE(FetchAndCacheSystemInfo());

  // TODO(crbug.com/1326674): Check for specific values once Fuchsia
  // completes the requested changes to the data returned from the fake.
  EXPECT_TRUE(GetCachedBuildInfo().has_product_config());
  EXPECT_TRUE(GetCachedBuildInfo().has_board_config());
  EXPECT_TRUE(GetCachedBuildInfo().has_version());
  EXPECT_TRUE(GetCachedBuildInfo().has_latest_commit_date());
}

}  // namespace base
