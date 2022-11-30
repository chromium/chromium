// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/usage_scenario/system_event_provider.h"

#include <memory>

#include "base/test/power_monitor_test.h"
#include "base/test/task_environment.h"
#include "chrome/browser/metrics/usage_scenario/usage_scenario_data_store.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(SystemEventProviderTest, OnSuspend) {
  base::test::TaskEnvironment task_environment;
  base::test::ScopedPowerMonitorTestSource power_monitor_source;
  UsageScenarioDataStoreImpl data_store;
  SystemEventProvider sys_event_provider(&data_store);

  EXPECT_EQ(0U, data_store.GetIntervalDataForTesting().sleep_events);
  power_monitor_source.GenerateSuspendEvent();
  EXPECT_EQ(1U, data_store.GetIntervalDataForTesting().sleep_events);
  power_monitor_source.GenerateResumeEvent();
  power_monitor_source.GenerateSuspendEvent();
  EXPECT_EQ(2U, data_store.GetIntervalDataForTesting().sleep_events);
}
