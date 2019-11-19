// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tracing/trace_event_system_stats_monitor.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/browser/performance_monitor/system_monitor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tracing {

using TraceSystemStatsMonitorTest = testing::Test;

TEST_F(TraceSystemStatsMonitorTest, TraceEventSystemStatsMonitor) {
  base::test::TaskEnvironment task_environment;

  auto system_monitor = performance_monitor::SystemMonitor::Create();
  TraceEventSystemStatsMonitor system_stats_monitor;

  EXPECT_TRUE(
      base::trace_event::TraceLog::GetInstance()->HasAsyncEnabledStateObserver(
          &system_stats_monitor));

  // By default the observer isn't dumping memory profiles.
  EXPECT_FALSE(system_stats_monitor.is_profiling_for_testing());

  // Simulate enabling tracing.
  system_stats_monitor.StartProfilingForTesting();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(system_stats_monitor.is_profiling_for_testing());

  // Simulate disabling tracing.
  system_stats_monitor.StopProfilingForTesting();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(system_stats_monitor.is_profiling_for_testing());
}

}  // namespace tracing
