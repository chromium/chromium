// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/notification_schedule_service_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "chrome/browser/notifications/scheduler/internal/notification_scheduler.h"
#include "chrome/browser/notifications/scheduler/internal/stats.h"
#include "chrome/browser/notifications/scheduler/public/notification_params.h"

namespace notifications {

NotificationScheduleServiceImpl::NotificationScheduleServiceImpl(
    std::unique_ptr<NotificationScheduler> scheduler)
    : scheduler_(std::move(scheduler)) {
  scheduler_->Init(
      base::BindOnce(&NotificationScheduleServiceImpl::OnInitialized,
                     weak_ptr_factory_.GetWeakPtr()));
}

NotificationScheduleServiceImpl::~NotificationScheduleServiceImpl() = default;

void NotificationScheduleServiceImpl::Schedule(
    std::unique_ptr<NotificationParams> notification_params) {
  scheduler_->Schedule(std::move(notification_params));
}

void NotificationScheduleServiceImpl::DeleteNotifications(
    SchedulerClientType type) {
  scheduler_->DeleteAllNotifications(type);
}

void NotificationScheduleServiceImpl::GetClientOverview(
    SchedulerClientType type,
    ClientOverview::ClientOverviewCallback callback) {
  scheduler_->GetClientOverview(type, std::move(callback));
}

NotificationBackgroundTaskScheduler::Handler*
NotificationScheduleServiceImpl::GetBackgroundTaskSchedulerHandler() {
  return this;
}

UserActionHandler* NotificationScheduleServiceImpl::GetUserActionHandler() {
  return this;
}

void NotificationScheduleServiceImpl::OnStartTask(
    TaskFinishedCallback callback) {
  scheduler_->OnStartTask(std::move(callback));
}

void NotificationScheduleServiceImpl::OnStopTask() {
  scheduler_->OnStopTask();
}

void NotificationScheduleServiceImpl::OnUserAction(
    const UserActionData& action_data) {
  stats::LogUserAction(action_data);
  scheduler_->OnUserAction(action_data);
}

void NotificationScheduleServiceImpl::OnInitialized(bool success) {
  // TODO(xingliu): Track metric here.
  NOTIMPLEMENTED();
}

}  // namespace notifications
