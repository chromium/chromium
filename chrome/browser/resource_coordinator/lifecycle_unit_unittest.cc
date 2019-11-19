// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/lifecycle_unit.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_coordinator {

TEST(LifecycleUnitTest, SortKeyComparison) {
  constexpr base::TimeTicks kBaseTime;
  LifecycleUnit::SortKey a(kBaseTime);
  LifecycleUnit::SortKey b(kBaseTime + base::TimeDelta::FromHours(1));
  LifecycleUnit::SortKey c(kBaseTime + base::TimeDelta::FromHours(2));

  EXPECT_FALSE(a < a);
  EXPECT_TRUE(a < b);
  EXPECT_TRUE(a < c);

  EXPECT_FALSE(b < a);
  EXPECT_FALSE(b < b);
  EXPECT_TRUE(b < c);

  EXPECT_FALSE(c < a);
  EXPECT_FALSE(c < b);
  EXPECT_FALSE(c < c);

  EXPECT_FALSE(a > a);
  EXPECT_FALSE(a > b);
  EXPECT_FALSE(a > c);

  EXPECT_TRUE(b > a);
  EXPECT_FALSE(b > b);
  EXPECT_FALSE(b > c);

  EXPECT_TRUE(c > a);
  EXPECT_TRUE(c > b);
  EXPECT_FALSE(c > c);
}

}  // namespace resource_coordinator
