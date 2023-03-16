// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/build_time.h"

#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(BuildTime, DateLooksValid) {
  base::Time build_time = base::GetBuildTime();
  base::Time::Exploded exploded_build_time;
  build_time.UTCExplode(&exploded_build_time);
  ASSERT_TRUE(exploded_build_time.HasValidValues());

#if !defined(OFFICIAL_BUILD)
  EXPECT_EQ(exploded_build_time.hour, 5);
  EXPECT_EQ(exploded_build_time.minute, 0);
  EXPECT_EQ(exploded_build_time.second, 0);
#endif
}

TEST(BuildTime, InThePast) {
  EXPECT_LT(base::GetBuildTime(), base::Time::Now());
  EXPECT_LT(base::GetBuildTime(), base::Time::NowFromSystemTime());
}
