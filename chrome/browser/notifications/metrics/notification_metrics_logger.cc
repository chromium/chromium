// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/metrics/notification_metrics_logger.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/notifications/metrics/notification_metrics_logger_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/common/persistent_notification_status.h"
#include "third_party/blink/public/mojom/notifications/notification.mojom.h"

NotificationMetricsLogger::NotificationMetricsLogger() = default;

NotificationMetricsLogger::~NotificationMetricsLogger() = default;

void NotificationMetricsLogger::LogPersistentNotificationClosedByUser() {
  base::RecordAction(
      base::UserMetricsAction("Notifications.Persistent.ClosedByUser"));
}

void NotificationMetricsLogger::
    LogPersistentNotificationClosedProgrammatically() {
  base::RecordAction(base::UserMetricsAction(
      "Notifications.Persistent.ClosedProgrammatically"));
}

void NotificationMetricsLogger::LogPersistentNotificationActionButtonClick() {
  base::RecordAction(
      base::UserMetricsAction("Notifications.Persistent.ClickedActionButton"));
}

void NotificationMetricsLogger::LogPersistentNotificationClick() {
  base::RecordAction(
      base::UserMetricsAction("Notifications.Persistent.Clicked"));
}

void NotificationMetricsLogger::
    LogPersistentNotificationClickWithoutPermission() {
  base::RecordAction(base::UserMetricsAction(
      "Notifications.Persistent.ClickedWithoutPermission"));
}

void NotificationMetricsLogger::LogPersistentNotificationShown() {
  base::RecordAction(base::UserMetricsAction("Notifications.Persistent.Shown"));
}

void NotificationMetricsLogger::LogPersistentNotificationSize(
    const Profile* profile,
    const blink::PlatformNotificationData& notification_data,
    const GURL& origin) {
  // This method should only be called for ESB users.
  DCHECK(safe_browsing::IsEnhancedProtectionEnabled(*profile->GetPrefs()));
  base::UmaHistogramCounts1000("Notifications.Persistent.Origin.SizeInBytes",
                               origin.spec().size());
  base::UmaHistogramCounts1000("Notifications.Persistent.Title.SizeInBytes",
                               notification_data.title.size());
  base::UmaHistogramCounts1M("Notifications.Persistent.Body.SizeInBytes",
                             notification_data.body.size());
  base::UmaHistogramCounts1000("Notifications.Persistent.Icon.SizeInBytes",
                               notification_data.icon.spec().size());
  base::UmaHistogramCounts1000("Notifications.Persistent.Image.SizeInBytes",
                               notification_data.image.spec().size());
  base::UmaHistogramCounts1000("Notifications.Persistent.Badge.SizeInBytes",
                               notification_data.badge.spec().size());
  base::UmaHistogramCounts1M("Notifications.Persistent.Data.SizeInBytes",
                             notification_data.data.size());
  for (const blink::mojom::NotificationActionPtr& action_ptr :
       notification_data.actions) {
    base::UmaHistogramCounts1000(
        "Notifications.Persistent.Actions.Icon.SizeInBytes",
        action_ptr->icon.spec().size());
    base::UmaHistogramCounts1000(
        "Notifications.Persistent.Actions.Action.SizeInBytes",
        action_ptr->action.size());
    base::UmaHistogramCounts1000(
        "Notifications.Persistent.Actions.Title.SizeInBytes",
        action_ptr->title.size());
    if (action_ptr->placeholder.has_value()) {
      base::UmaHistogramCounts1000(
          "Notifications.Persistent.Actions.Placeholder.SizeInBytes",
          action_ptr->placeholder.value().size());
    }
  }
}
