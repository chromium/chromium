// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/stats.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "chrome/browser/notifications/scheduler/public/notification_data.h"

namespace notifications {
namespace stats {
namespace {

// Returns the histogram suffix for a client type. Should match suffix
// NotificationSchedulerClientType in histograms.xml.
std::string ToHistogramSuffix(SchedulerClientType client_type) {
  switch (client_type) {
    case SchedulerClientType::kTest1:
    case SchedulerClientType::kTest2:
    case SchedulerClientType::kTest3:
      return "__Test__";
    case SchedulerClientType::kUnknown:
    case SchedulerClientType::kDeprecatedFeatureGuide:
      return "Unknown";
    case SchedulerClientType::kWebUI:
      return "WebUI";
    case SchedulerClientType::kChromeUpdate:
      return "ChromeUpdate";
    case SchedulerClientType::kPrefetch:
      return "Prefetch";
    case SchedulerClientType::kReadingList:
      return "ReadingList";
  }
}

// Logs a histogram enumeration with client type suffix.
template <typename T>
void LogHistogramEnumWithSuffix(const std::string& name,
                                T value,
                                SchedulerClientType client_type) {
  base::UmaHistogramEnumeration(name, value);
  auto name_with_suffix = name;
  name_with_suffix.append(".").append(ToHistogramSuffix(client_type));
  base::UmaHistogramEnumeration(name_with_suffix, value);
}

}  // namespace

void LogUserAction(const UserActionData& action_data) {

  // Logs inline helpful/unhelpful buttons clicks.
  if (action_data.button_click_info.has_value()) {
    switch (action_data.button_click_info->type) {
      case ActionButtonType::kHelpful:
        break;
      case ActionButtonType::kUnhelpful:
        break;
      case ActionButtonType::kUnknownAction:
        break;
    }
  }
}

void LogBackgroundTaskNotificationShown(int shown_count) {
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Notifications.Scheduler.BackgroundTask.NotificationShown", shown_count,
      0, 10, 11);
}

void LogNotificationShow(const NotificationData& notification_data,
                         SchedulerClientType client_type) {
  LogNotificationLifeCycleEvent(NotificationLifeCycleEvent::kShown,
                                client_type);
}

void LogNotificationLifeCycleEvent(NotificationLifeCycleEvent event,
                                   SchedulerClientType client_type) {
  LogHistogramEnumWithSuffix(
      "Notifications.Scheduler.NotificationLifeCycleEvent", event, client_type);
}
}  // namespace stats
}  // namespace notifications
