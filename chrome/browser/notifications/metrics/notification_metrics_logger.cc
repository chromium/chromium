// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/metrics/notification_metrics_logger.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/notifications/metrics/notification_metrics_logger_factory.h"
#include "content/public/common/persistent_notification_status.h"

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
