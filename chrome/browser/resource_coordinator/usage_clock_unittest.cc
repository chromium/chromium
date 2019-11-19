// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/usage_clock.h"

#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_tracker.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_coordinator {

TEST(ResourceCoordinatorUsageClock, UsageClock) {
  // Required to use DesktopSessionDurationTracker.
  base::test::TaskEnvironment task_environment;

  {
    base::SimpleTestTickClock clock;
    clock.Advance(base::TimeDelta::FromMinutes(42));
    ScopedSetTickClockForTesting scoped_set_tick_clock_for_testing(&clock);

    metrics::DesktopSessionDurationTracker::Initialize();
    auto* tracker = metrics::DesktopSessionDurationTracker::Get();
    tracker->OnVisibilityChanged(true, base::TimeDelta());
    tracker->OnUserEvent();
    EXPECT_TRUE(tracker->in_session());

    UsageClock usage_clock;
    EXPECT_EQ(usage_clock.GetTotalUsageTime(), base::TimeDelta());
    EXPECT_TRUE(tracker->in_session());
    EXPECT_TRUE(usage_clock.IsInUse());

    // Verify that time advances when Chrome is in use.
    clock.Advance(base::TimeDelta::FromMinutes(1));
    EXPECT_EQ(usage_clock.GetTotalUsageTime(), base::TimeDelta::FromMinutes(1));
    clock.Advance(base::TimeDelta::FromMinutes(1));
    EXPECT_EQ(usage_clock.GetTotalUsageTime(), base::TimeDelta::FromMinutes(2));

    // Verify that time is updated when Chrome stops being used.
    clock.Advance(base::TimeDelta::FromMinutes(1));
    tracker->OnVisibilityChanged(false, base::TimeDelta());
    EXPECT_FALSE(tracker->in_session());
    EXPECT_FALSE(usage_clock.IsInUse());
    EXPECT_EQ(usage_clock.GetTotalUsageTime(), base::TimeDelta::FromMinutes(3));

    // Verify that time stays still when Chrome is not in use.
    clock.Advance(base::TimeDelta::FromMinutes(1));
    EXPECT_EQ(usage_clock.GetTotalUsageTime(), base::TimeDelta::FromMinutes(3));

    // Verify that time advances again when Chrome is in use.
    tracker->OnVisibilityChanged(true, base::TimeDelta());
    EXPECT_TRUE(tracker->in_session());
    EXPECT_TRUE(usage_clock.IsInUse());
    clock.Advance(base::TimeDelta::FromMinutes(1));
    EXPECT_EQ(usage_clock.GetTotalUsageTime(), base::TimeDelta::FromMinutes(4));
  }

  // Must be after UsageClock destruction.
  metrics::DesktopSessionDurationTracker::CleanupForTesting();
}

}  // namespace resource_coordinator
