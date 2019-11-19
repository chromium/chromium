// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/stats.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/notifications/scheduler/public/notification_data.h"

namespace notifications {
namespace stats {
namespace {

const char kIhnrActionButtonEventHistogram[] =
    "Notifications.Scheduler.IhnrActionButtonEvent";

// Returns the histogram suffix for a client type. Should match suffix
// NotificationSchedulerClientType in histograms.xml.
std::string ToHistogramSuffix(SchedulerClientType client_type) {
  switch (client_type) {
    case SchedulerClientType::kTest1:
    case SchedulerClientType::kTest2:
    case SchedulerClientType::kTest3:
      return "__Test__";
    case SchedulerClientType::kUnknown:
      return "Unknown";
    case SchedulerClientType::kWebUI:
      return "WebUI";
    case SchedulerClientType::kChromeUpdate:
      return "ChromeUpdate";
  }
}

// Returns the string representing database type.
std::string ToDbTypeString(DatabaseType type) {
  switch (type) {
    case DatabaseType::kImpressionDb:
      return "ImpressionDb";
    case DatabaseType::kNotificationDb:
      return "NotificationDb";
    case DatabaseType::kIconDb:
      return "IconDb";
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
  // Logs action type.
  LogHistogramEnumWithSuffix("Notifications.Scheduler.UserAction",
                             action_data.action_type, action_data.client_type);

  // Logs inline helpful/unhelpful buttons clicks.
  if (action_data.button_click_info.has_value()) {
    switch (action_data.button_click_info->type) {
      case ActionButtonType::kHelpful:
        LogHistogramEnumWithSuffix(kIhnrActionButtonEventHistogram,
                                   ActionButtonEvent::kHelpfulClick,
                                   action_data.client_type);
        break;
      case ActionButtonType::kUnhelpful:
        LogHistogramEnumWithSuffix(kIhnrActionButtonEventHistogram,
                                   ActionButtonEvent::kUnhelpfulClick,
                                   action_data.client_type);
        break;
      case ActionButtonType::kUnknownAction:
        break;
    }
  }
}

void LogBackgroundTaskEvent(BackgroundTaskEvent event) {
  UMA_HISTOGRAM_ENUMERATION("Notifications.Scheduler.BackgroundTask.Event",
                            event);

  if (event == BackgroundTaskEvent::kStart) {
    base::Time::Exploded explode;
    base::Time::Now().LocalExplode(&explode);
    UMA_HISTOGRAM_EXACT_LINEAR("Notifications.Scheduler.BackgroundTask.Start",
                               explode.hour, 24);
  }
}

void LogBackgroundTaskNotificationShown(int shown_count) {
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Notifications.Scheduler.BackgroundTask.NotificationShown", shown_count,
      0, 10, 11);
}

void LogDbInit(DatabaseType type, bool success, int entry_count) {
  std::string prefix("Notifications.Scheduler.");
  prefix.append(ToDbTypeString(type));
  std::string init_histogram_name = prefix;
  init_histogram_name.append(".InitResult");
  base::UmaHistogramBoolean(init_histogram_name, success);

  std::string record_count_name = prefix;
  record_count_name.append(".RecordCount");
  base::UmaHistogramCounts100(record_count_name, entry_count);
}

void LogDbOperation(DatabaseType type, bool success) {
  std::string name("Notifications.Scheduler.");
  name.append(ToDbTypeString(type)).append(".OperationResult");
  base::UmaHistogramBoolean(name, success);
}

void LogImpressionCount(int impression_count, SchedulerClientType type) {
  std::string name("Notifications.Scheduler.Impression.Count.");
  name.append(ToHistogramSuffix(type));
  base::UmaHistogramCounts100(name, impression_count);
}

void LogImpressionEvent(ImpressionEvent event) {
  UMA_HISTOGRAM_ENUMERATION("Notifications.Scheduler.Impression.Event", event);
}

void LogNotificationShow(const NotificationData& notification_data,
                         SchedulerClientType client_type) {
  bool has_ihnr_button = false;
  for (const auto& button : notification_data.buttons) {
    if (button.type == ActionButtonType::kHelpful ||
        button.type == ActionButtonType::kUnhelpful) {
      has_ihnr_button = true;
      break;
    }
  }

  if (has_ihnr_button) {
    LogHistogramEnumWithSuffix(kIhnrActionButtonEventHistogram,
                               ActionButtonEvent::kShown, client_type);
  }

  LogNotificationLifeCycleEvent(NotificationLifeCycleEvent::kShown,
                                client_type);
}

void LogNotificationLifeCycleEvent(NotificationLifeCycleEvent event,
                                   SchedulerClientType client_type) {
  LogHistogramEnumWithSuffix(
      "Notifications.Scheduler.NotificationLifeCycleEvent", event, client_type);
}

void LogPngIconConverterEncodeResult(bool success) {
  UMA_HISTOGRAM_BOOLEAN("Notifications.Scheduler.PngIconConverter.EncodeResult",
                        success);
}

void LogPngIconConverterDecodeResult(bool success) {
  UMA_HISTOGRAM_BOOLEAN("Notifications.Scheduler.PngIconConverter.DecodeResult",
                        success);
}
}  // namespace stats
}  // namespace notifications
