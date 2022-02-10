// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/power/coalition_resource_usage_provider_mac.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/browser/metrics/power/coalition_resource_usage_provider_test_util_mac.h"
#include "components/power_metrics/resource_coalition_mac.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(CoalitionResourceUsageProviderTest, Availability) {
  base::HistogramTester histogram_tester;
  CoalitionResourceUsageProvider provider;
  provider.Init();
  // Tests are usually run from a terminal and so they share their coalition ID
  // with it. This will fail if the tests is started with |launchd| or with
  // |open|.
  histogram_tester.ExpectUniqueSample(
      "PerformanceMonitor.ResourceCoalition.Availability",
      4 /* kNotAloneInCoalition */, 1);
}

// Test that resource usage rate is as expected.
//
// Since the bulk of the work is done by
// `power_metrics::GetCoalitionResourceUsageRate` which is tested elsewhere,
// this test only has expectations for one field. This test is still important
// to ensure that data is sampled and passed to
// `power_metrics::GetCoalitionResourceUsageRate` correctly.
TEST(CoalitionResourceUsageProviderTest, GetCoalitionResourceUsageRate) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  TestCoalitionResourceUsageProvider provider;

  auto cru1 = std::make_unique<coalition_resource_usage>();
  cru1->interrupt_wakeups = 10;
  provider.SetCoalitionResourceUsage(std::move(cru1));
  provider.Init();

  task_environment.FastForwardBy(base::Seconds(10));
  auto cru2 = std::make_unique<coalition_resource_usage>();
  cru2->interrupt_wakeups = 42;
  provider.SetCoalitionResourceUsage(std::move(cru2));
  auto rate1 = provider.GetCoalitionResourceUsageRate();
  ASSERT_TRUE(rate1.has_value());
  EXPECT_EQ(rate1->interrupt_wakeups_per_second, 3.2);

  task_environment.FastForwardBy(base::Seconds(20));
  auto cru3 = std::make_unique<coalition_resource_usage>();
  cru3->interrupt_wakeups = 100;
  provider.SetCoalitionResourceUsage(std::move(cru3));
  auto rate2 = provider.GetCoalitionResourceUsageRate();
  ASSERT_TRUE(rate2.has_value());
  EXPECT_EQ(rate2->interrupt_wakeups_per_second, 2.9);
}
