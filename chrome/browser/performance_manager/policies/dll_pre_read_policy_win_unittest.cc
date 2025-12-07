// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/public/dll_pre_read_policy_win.h"

#include "base/files/drive_info.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/browser_features.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/startup_metric_utils/common/startup_metric_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

TEST(DllPreReadPolicyTest, TestIsFixedSsd) {
  std::optional<base::DriveInfo> info = base::DriveInfo{};
  EXPECT_FALSE(IsFixedSsd(info));

  info->is_usb = false;
  EXPECT_FALSE(IsFixedSsd(info));

  info->is_removable = false;
  EXPECT_FALSE(IsFixedSsd(info));

  info->has_seek_penalty = false;
  EXPECT_TRUE(IsFixedSsd(info));

  info->is_usb = true;
  EXPECT_FALSE(IsFixedSsd(info));
  info->is_usb = false;
  info->is_removable = true;
  EXPECT_FALSE(IsFixedSsd(info));
}

TEST(DllPreReadPolicyTest, ShouldPrefetch) {
  base::test::ScopedFeatureList list(features::kNoPreReadMainDllIfSsd);
  SetChromeDllOnSsdForTesting(false);

  EXPECT_TRUE(ShouldPreReadDllInChild());
}

TEST(DllPreReadPolicyTest, FixedSsdShouldNotPrefetch) {
  base::test::ScopedFeatureList list(features::kNoPreReadMainDllIfSsd);
  SetChromeDllOnSsdForTesting(true);

  EXPECT_FALSE(ShouldPreReadDllInChild());
}

TEST(DllPreReadPolicyTest, StartupPrefetch) {
  base::test::ScopedFeatureList list(features::kNoPreReadMainDllStartup);
  base::TimeTicks now = base::TimeTicks::Now();
  startup_metric_utils::GetCommon().RecordChromeMainEntryTime(now);
  EXPECT_FALSE(StartupPrefetchTimeoutElapsed(now));
  EXPECT_TRUE(StartupPrefetchTimeoutElapsed(now + base::Minutes(5)));
}

TEST(DllPreReadPolicyTest, NoPreReadMainDll) {
  base::test::ScopedFeatureList list(features::kNoPreReadMainDll);
  EXPECT_FALSE(ShouldPreReadDllInChild());
}

}  // namespace performance_manager
