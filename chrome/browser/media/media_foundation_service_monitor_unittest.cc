// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_foundation_service_monitor.h"

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "media/base/media_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Test the `days_since_last_disabling_date` against `disabled_dates`, both of
// which are integer(s) number of days since an arbitrary base time.
void TestEarliestEnableDate(std::vector<int> disabled_dates,
                            int days_since_last_disabling_date) {
  // An arbitrary base time for the tests.
  base::Time base_time;
  EXPECT_TRUE(base::Time::FromString("22 Sep 2022 12:23 GMT", &base_time));

  std::vector<base::Time> disabled_times;
  for (const auto& days : disabled_dates) {
    disabled_times.push_back(base_time + base::Days(days));
  }
  auto enable_time =
      MediaFoundationServiceMonitor::GetEarliestEnableTime(disabled_times);
  auto expected_time =
      disabled_times.back() + base::Days(days_since_last_disabling_date);

  // Expect the `enable_time` to be in a range to avoid testing rounding logic.
  EXPECT_LE(enable_time, expected_time + base::Days(1));
  EXPECT_GE(enable_time, expected_time - base::Days(1));
}

}  // namespace

TEST(MediaFoundationServiceMonitorTest, GetEarliestEnableTime_Default) {
  // One disabling event will cause the feature to be disabled for 30 days,
  // which is the minimum disabling days.
  TestEarliestEnableDate({0}, 30);
  TestEarliestEnableDate({1}, 30);
  TestEarliestEnableDate({10}, 30);

  // Two close disabling events will cause the feature to be disabled for 180
  // days, which is the maximum disabling days.
  TestEarliestEnableDate({10, 10}, 180);
  TestEarliestEnableDate({10, 20}, 180);
  TestEarliestEnableDate({10, 40}, 180);

  // The closer the two disabling events are, the longer the feature will be
  // disabled.
  TestEarliestEnableDate({10, 50}, 142);
  TestEarliestEnableDate({10, 100}, 80);
  TestEarliestEnableDate({10, 1000}, 34);

  // Two far apart disabling events will cause the feature to be disabled for 30
  // days, which is the minimum disabling days.
  TestEarliestEnableDate({10, 10000}, 30);

  // The third disabling event time doesn't matter.
  TestEarliestEnableDate({10, 50}, 142);
  TestEarliestEnableDate({1, 10, 50}, 142);
}

TEST(MediaFoundationServiceMonitorTest, GetEarliestEnableTime_Overridden) {
  // Ensure we take any base::Feature overrides into account.
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      media::kHardwareSecureDecryptionFallback,
      {{"min_disabling_days", "10"}, {"max_disabling_days", "60"}});

  // One disabling event will cause the feature to be disabled for 30 days,
  // which is the minimum disabling days.
  TestEarliestEnableDate({0}, 10);
  TestEarliestEnableDate({1}, 10);
  TestEarliestEnableDate({10}, 10);

  // Two close disabling events will cause the feature to be disabled for 60
  // days, which is the maximum disabling days.
  TestEarliestEnableDate({10, 10}, 60);
  TestEarliestEnableDate({10, 20}, 60);

  // The closer the two disabling events are, the longer the feature will be
  // disabled.
  TestEarliestEnableDate({10, 40}, 26);
  TestEarliestEnableDate({10, 50}, 22);
  TestEarliestEnableDate({10, 100}, 15);

  // Two far apart disabling events will cause the feature to be disabled for 10
  // days, which is the minimum disabling days.
  TestEarliestEnableDate({10, 1000}, 10);
  TestEarliestEnableDate({10, 10000}, 10);

  // The third disabling event time doesn't matter.
  TestEarliestEnableDate({10, 50}, 22);
  TestEarliestEnableDate({1, 10, 50}, 22);
}
