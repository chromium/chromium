// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/scheduler_config.h"

#include <map>
#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/notifications/scheduler/public/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace notifications {
namespace {

TEST(SchedulerConfigTest, FinchConfigTest) {
  base::test::ScopedFeatureList scoped_feature_list;
  std::map<std::string, std::string> parameters = {
      {kMaxDailyShownAllTypeConfig, base::NumberToString(123)},
      {kMaxDailyShownPerTypeConfig, base::NumberToString(67)},
      {kInitialDailyShownPerTypeConfig, base::NumberToString(45)},
      {kNotificationExpirationConfig, base::NumberToString(33)},
      {kImpressionExpirationConfig, base::NumberToString(22)},
      {kSuppressionDurationConfig, base::NumberToString(11)},
      {kDismissCountConfig, base::NumberToString(8)},
      {kDismissDurationConfig, base::NumberToString(7)},
      {kBackgroundTaskWindowDurationConfig, base::NumberToString(6)},
  };
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kNotificationScheduleService, parameters);
  std::unique_ptr<SchedulerConfig> config = SchedulerConfig::CreateFromFinch();

  EXPECT_EQ(config->max_daily_shown_all_type, 123);
  EXPECT_EQ(config->max_daily_shown_per_type, 67);
  EXPECT_EQ(config->initial_daily_shown_per_type, 45);
  EXPECT_EQ(config->notification_expiration.InDays(), 33);
  EXPECT_EQ(config->impression_expiration.InDays(), 22);
  EXPECT_EQ(config->suppression_duration.InDays(), 11);
  EXPECT_EQ(config->dismiss_count, 8);
  EXPECT_EQ(config->background_task_window_duration.InHours(), 6);
}

}  // namespace
}  // namespace notifications
