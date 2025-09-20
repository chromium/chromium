// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/public/schedule_service_utils.h"

#include <map>

#include "base/check_op.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_types.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace notifications {
namespace {

const std::map<TipsNotificationsFeatureType, std::pair<int, int>>&
GetTipsNotificationsFeatureTypeMap() {
  static const base::NoDestructor<
      std::map<TipsNotificationsFeatureType, std::pair<int, int>>>
      kTipsNotificationsFeatureTypeMap({
          {TipsNotificationsFeatureType::kEnhancedSafeBrowsing,
           {IDS_TIPS_NOTIFICATIONS_ENHANCED_SAFE_BROWSING_TITLE,
            IDS_TIPS_NOTIFICATIONS_ENHANCED_SAFE_BROWSING_SUBTITLE}},
          {TipsNotificationsFeatureType::kQuickDelete,
           {IDS_TIPS_NOTIFICATIONS_QUICK_DELETE_TITLE,
            IDS_TIPS_NOTIFICATIONS_QUICK_DELETE_SUBTITLE}},
          {TipsNotificationsFeatureType::kGoogleLens,
           {IDS_TIPS_NOTIFICATIONS_GOOGLE_LENS_TITLE,
            IDS_TIPS_NOTIFICATIONS_GOOGLE_LENS_SUBTITLE}},
          {TipsNotificationsFeatureType::kBottomOmnibox,
           {IDS_TIPS_NOTIFICATIONS_BOTTOM_OMNIBOX_TITLE,
            IDS_TIPS_NOTIFICATIONS_BOTTOM_OMNIBOX_SUBTITLE}},
      });
  return *kTipsNotificationsFeatureTypeMap;
}

bool ValidateTimeWindow(const TimeDeltaPair& window) {
  return (window.second - window.first < base::Hours(12) &&
          window.second >= window.first);
}

}  // namespace

bool ToLocalHour(int hour,
                 const base::Time& today,
                 int day_delta,
                 base::Time* out) {
  DCHECK_GE(hour, 0);
  DCHECK_LE(hour, 23);
  DCHECK(out);

  // Gets the local time at |hour| in yesterday.
  base::Time another_day = today + base::Days(day_delta);
  base::Time::Exploded another_day_exploded;
  another_day.LocalExplode(&another_day_exploded);
  another_day_exploded.hour = hour;
  another_day_exploded.minute = 0;
  another_day_exploded.second = 0;
  another_day_exploded.millisecond = 0;

  // Converts local exploded time to time stamp.
  return base::Time::FromLocalExploded(another_day_exploded, out);
}

bool NextTimeWindow(base::Clock* clock,
                    const TimeDeltaPair& morning,
                    const TimeDeltaPair& evening,
                    TimePair* out) {
  auto now = clock->Now();
  base::Time beginning_of_today;
  // verify the inputs.
  if (!ToLocalHour(0, now, 0, &beginning_of_today) ||
      !ValidateTimeWindow(morning) || !ValidateTimeWindow(evening) ||
      morning.second > evening.first) {
    return false;
  }

  auto today_morning_window = std::pair<base::Time, base::Time>(
      beginning_of_today + morning.first, beginning_of_today + morning.second);
  if (now <= today_morning_window.second) {
    *out = std::move(today_morning_window);
    return true;
  }

  auto today_evening_window = std::pair<base::Time, base::Time>(
      beginning_of_today + evening.first, beginning_of_today + evening.second);
  if (now <= today_evening_window.second) {
    *out = std::move(today_evening_window);
    return true;
  }

  // tomorrow morning window.
  *out = std::pair<base::Time, base::Time>(
      beginning_of_today + base::Days(1) + morning.first,
      beginning_of_today + base::Days(1) + morning.second);
  return true;
}

NotificationData GetTipsNotificationData(
    TipsNotificationsFeatureType feature_type) {
  const auto& map = GetTipsNotificationsFeatureTypeMap();
  const auto it = map.find(feature_type);
  DCHECK(it != map.end());

  NotificationData data;
  data.title = l10n_util::GetStringUTF16(it->second.first);
  data.message = l10n_util::GetStringUTF16(it->second.second);
  data.custom_data[kTipsNotificationsFeatureType] =
      base::NumberToString(static_cast<int>(feature_type));
  data.buttons.clear();
  NotificationData::Button open_chrome_button;
  open_chrome_button.type = ActionButtonType::kHelpful;
  open_chrome_button.id = kDefaultHelpfulButtonId;
  open_chrome_button.text =
      l10n_util::GetStringUTF16(IDS_TIPS_NOTIFICATIONS_HELPFUL_BUTTON_TEXT);
  data.buttons.emplace_back(std::move(open_chrome_button));
  return data;
}

}  // namespace notifications
