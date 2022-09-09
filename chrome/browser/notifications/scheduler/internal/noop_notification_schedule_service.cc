// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/noop_notification_schedule_service.h"

#include "chrome/browser/notifications/scheduler/public/notification_params.h"

namespace notifications {

NoopNotificationScheduleService::NoopNotificationScheduleService() = default;

NoopNotificationScheduleService::~NoopNotificationScheduleService() = default;

void NoopNotificationScheduleService::Schedule(
    std::unique_ptr<NotificationParams> notification_params) {}

void NoopNotificationScheduleService::DeleteNotifications(
    SchedulerClientType type) {}

void NoopNotificationScheduleService::GetClientOverview(
    SchedulerClientType,
    ClientOverview::ClientOverviewCallback callback) {}

NotificationBackgroundTaskScheduler::Handler*
NoopNotificationScheduleService::GetBackgroundTaskSchedulerHandler() {
  return this;
}

UserActionHandler* NoopNotificationScheduleService::GetUserActionHandler() {
  return this;
}

void NoopNotificationScheduleService::OnStartTask(
    TaskFinishedCallback callback) {}

void NoopNotificationScheduleService::OnStopTask() {}

void NoopNotificationScheduleService::OnUserAction(
    const UserActionData& action_data) {}

}  // namespace notifications
