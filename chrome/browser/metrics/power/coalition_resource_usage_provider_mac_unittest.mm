// Copyright 2021 The Chromium Authors
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
TEST(CoalitionResourceUsageProviderTest, StartAndEndIntervals) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  TestCoalitionResourceUsageProvider provider;

  absl::optional<power_metrics::CoalitionResourceUsageRate> short_rate;
  absl::optional<power_metrics::CoalitionResourceUsageRate> long_rate;

  // Begin long interval.
  auto cru1 = std::make_unique<coalition_resource_usage>();
  cru1->interrupt_wakeups = 8;
  provider.SetCoalitionResourceUsage(std::move(cru1));
  provider.Init();

  // Begin short interval.
  task_environment.FastForwardBy(base::Seconds(10));
  auto cru2 = std::make_unique<coalition_resource_usage>();
  cru2->interrupt_wakeups = 42;
  provider.SetCoalitionResourceUsage(std::move(cru2));
  provider.StartShortInterval();

  // End long and short intervals. Start long interval.
  task_environment.FastForwardBy(base::Seconds(5));
  auto cru3 = std::make_unique<coalition_resource_usage>();
  cru3->interrupt_wakeups = 50;
  provider.SetCoalitionResourceUsage(std::move(cru3));
  provider.EndIntervals(&short_rate, &long_rate);
  ASSERT_TRUE(short_rate.has_value());
  ASSERT_TRUE(long_rate.has_value());
  EXPECT_EQ(short_rate->interrupt_wakeups_per_second, 1.6);
  EXPECT_EQ(long_rate->interrupt_wakeups_per_second, 2.8);
  short_rate.reset();
  long_rate.reset();

  // Begin short interval.
  task_environment.FastForwardBy(base::Seconds(100));
  auto cru4 = std::make_unique<coalition_resource_usage>();
  cru4->interrupt_wakeups = 100;
  provider.SetCoalitionResourceUsage(std::move(cru4));
  provider.StartShortInterval();

  // End long and short intervals. Start long interval.
  task_environment.FastForwardBy(base::Seconds(25));
  auto cru5 = std::make_unique<coalition_resource_usage>();
  cru5->interrupt_wakeups = 170;
  provider.SetCoalitionResourceUsage(std::move(cru5));
  provider.EndIntervals(&short_rate, &long_rate);
  ASSERT_TRUE(short_rate.has_value());
  ASSERT_TRUE(long_rate.has_value());
  EXPECT_EQ(short_rate->interrupt_wakeups_per_second, 2.8);
  EXPECT_EQ(long_rate->interrupt_wakeups_per_second, 0.96);
}

// Test that there is no crash if `GetCoalitionResourceUsage()` returns nullptr.
// Regression test for crbug.com/1298733
TEST(CoalitionResourceUsageProviderTest, CoalitionResourceUsageIsNull) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  TestCoalitionResourceUsageProvider provider;

  absl::optional<power_metrics::CoalitionResourceUsageRate> short_rate;
  absl::optional<power_metrics::CoalitionResourceUsageRate> long_rate;

  // Begin long interval.
  provider.SetCoalitionResourceUsage(
      std::make_unique<coalition_resource_usage>());
  provider.Init();

  // Begin short interval. `GetCoalitionResourceUsage()` is nullptr.
  task_environment.FastForwardBy(base::Seconds(1));
  provider.StartShortInterval();

  // End both intervals and start long interval. `GetCoalitionResourceUsage()`
  // is nullptr.
  task_environment.FastForwardBy(base::Seconds(1));
  provider.EndIntervals(&short_rate, &long_rate);
  EXPECT_FALSE(short_rate.has_value());
  EXPECT_FALSE(long_rate.has_value());
  short_rate.reset();
  long_rate.reset();

  // Begin short interval.
  task_environment.FastForwardBy(base::Seconds(1));
  provider.SetCoalitionResourceUsage(
      std::make_unique<coalition_resource_usage>());
  provider.StartShortInterval();

  // End both intervals and start long interval.
  task_environment.FastForwardBy(base::Seconds(1));
  provider.SetCoalitionResourceUsage(
      std::make_unique<coalition_resource_usage>());
  provider.EndIntervals(&short_rate, &long_rate);
  EXPECT_TRUE(short_rate.has_value());
  EXPECT_FALSE(long_rate.has_value());
  short_rate.reset();
  long_rate.reset();

  // Begin short interval. `GetCoalitionResourceUsage()` is nullptr.
  task_environment.FastForwardBy(base::Seconds(1));
  provider.StartShortInterval();

  // End both intervals and start long interval.
  task_environment.FastForwardBy(base::Seconds(1));
  provider.SetCoalitionResourceUsage(
      std::make_unique<coalition_resource_usage>());
  provider.EndIntervals(&short_rate, &long_rate);
  EXPECT_FALSE(short_rate.has_value());
  EXPECT_TRUE(long_rate.has_value());
}
