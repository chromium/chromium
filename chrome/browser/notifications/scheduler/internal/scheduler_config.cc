// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/scheduler_config.h"

#include <string>

#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/notifications/scheduler/public/features.h"

namespace notifications {
namespace {

// Helper routine to get Finch experiment parameter. If no Finch seed was
// found,
// use the |default_value|. The |name| should match an experiment
// parameter in Finch server configuration.
int GetFinchConfigUInt(const std::string& name, int default_value) {
  return base::GetFieldTrialParamByFeatureAsInt(
      features::kNotificationScheduleService, name, default_value);
}

}  // namespace

// Default number of maximum daily shown all type.
constexpr int kDefaultMaxDailyShownAllType = 3;

// Default number of maximum daily shown per type.
constexpr int kDefaultMaxDailyShownPerType = 10;

// Default number of initial daily shown per type.
constexpr int kDefaultInitialDailyShownPerType = 2;

// Default number of dismiss count.
constexpr int kDefaultDismissCount = 3;

// The notification data is hold for one week.
constexpr base::TimeDelta kDefaultNotificationExpiration =
    base::TimeDelta::FromDays(7);

// The impression history is hold for 4 weeks.
constexpr base::TimeDelta kDefaultImpressionExpiration =
    base::TimeDelta::FromDays(28);

// The suppression lasts 8 weeks.
constexpr base::TimeDelta kDefaultSuppressionDuration =
    base::TimeDelta::FromDays(56);

// Check consecutive notification dismisses in this duration to generate a
// dismiss event.
constexpr base::TimeDelta kDefaultDismissDuration =
    base::TimeDelta::FromDays(7);

// Default background task time window duration.
constexpr base::TimeDelta kDefaultBackgroundTaskWindowDuration =
    base::TimeDelta::FromHours(1);

// static
std::unique_ptr<SchedulerConfig> SchedulerConfig::Create() {
  return std::make_unique<SchedulerConfig>();
}

std::unique_ptr<SchedulerConfig> SchedulerConfig::CreateFromFinch() {
  std::unique_ptr<SchedulerConfig> config = std::make_unique<SchedulerConfig>();
  config->max_daily_shown_all_type =
      base::saturated_cast<int>(GetFinchConfigUInt(
          kMaxDailyShownAllTypeConfig, kDefaultMaxDailyShownAllType));
  config->max_daily_shown_per_type =
      base::saturated_cast<int>(GetFinchConfigUInt(
          kMaxDailyShownPerTypeConfig, kDefaultMaxDailyShownPerType));
  config->initial_daily_shown_per_type =
      base::saturated_cast<int>(GetFinchConfigUInt(
          kInitialDailyShownPerTypeConfig, kDefaultInitialDailyShownPerType));
  config->notification_expiration =
      base::TimeDelta::FromDays(base::saturated_cast<int>(
          GetFinchConfigUInt(kNotificationExpirationConfig,
                             kDefaultNotificationExpiration.InDays())));
  config->impression_expiration =
      base::TimeDelta::FromDays(base::saturated_cast<int>(GetFinchConfigUInt(
          kImpressionExpirationConfig, kDefaultImpressionExpiration.InDays())));
  config->suppression_duration =
      base::TimeDelta::FromDays(base::saturated_cast<int>(GetFinchConfigUInt(
          kSuppressionDurationConfig, kDefaultSuppressionDuration.InDays())));
  config->dismiss_count = base::saturated_cast<int>(
      GetFinchConfigUInt(kDismissCountConfig, kDefaultDismissCount));
  config->dismiss_duration =
      base::TimeDelta::FromDays(base::saturated_cast<int>(GetFinchConfigUInt(
          kDismissDurationConfig, kDefaultDismissDuration.InDays())));
  config->background_task_window_duration =
      base::TimeDelta::FromHours(base::saturated_cast<int>(
          GetFinchConfigUInt(kBackgroundTaskWindowDurationConfig,
                             kDefaultBackgroundTaskWindowDuration.InHours())));
  return config;
}

SchedulerConfig::SchedulerConfig()
    : max_daily_shown_all_type(kDefaultMaxDailyShownAllType),
      max_daily_shown_per_type(kDefaultMaxDailyShownPerType),
      initial_daily_shown_per_type(kDefaultInitialDailyShownPerType),
      notification_expiration(kDefaultNotificationExpiration),
      impression_expiration(kDefaultImpressionExpiration),
      suppression_duration(kDefaultSuppressionDuration),
      dismiss_count(kDefaultDismissCount),
      dismiss_duration(kDefaultDismissDuration),
      background_task_window_duration(kDefaultBackgroundTaskWindowDuration) {}

SchedulerConfig::~SchedulerConfig() = default;

}  // namespace notifications
