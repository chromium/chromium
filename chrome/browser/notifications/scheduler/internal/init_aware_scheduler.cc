// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/init_aware_scheduler.h"

#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/notifications/scheduler/public/notification_params.h"

namespace notifications {

InitAwareNotificationScheduler::InitAwareNotificationScheduler(
    std::unique_ptr<NotificationScheduler> impl)
    : impl_(std::move(impl)) {}

InitAwareNotificationScheduler::~InitAwareNotificationScheduler() = default;

void InitAwareNotificationScheduler::Init(InitCallback init_callback) {
  DCHECK(!init_success_.has_value());
  impl_->Init(base::BindOnce(&InitAwareNotificationScheduler::OnInitialized,
                             weak_ptr_factory_.GetWeakPtr(),
                             std::move(init_callback)));
}

void InitAwareNotificationScheduler::Schedule(
    std::unique_ptr<NotificationParams> params) {
  if (IsReady()) {
    impl_->Schedule(std::move(params));
    return;
  }
  MaybeCacheClosure(base::BindOnce(&InitAwareNotificationScheduler::Schedule,
                                   weak_ptr_factory_.GetWeakPtr(),
                                   std::move(params)));
}

void InitAwareNotificationScheduler::DeleteAllNotifications(
    SchedulerClientType type) {
  if (IsReady()) {
    impl_->DeleteAllNotifications(type);
    return;
  }
  MaybeCacheClosure(
      base::BindOnce(&InitAwareNotificationScheduler::DeleteAllNotifications,
                     weak_ptr_factory_.GetWeakPtr(), type));
}

void InitAwareNotificationScheduler::GetClientOverview(
    SchedulerClientType type,
    ClientOverview::ClientOverviewCallback callback) {
  if (IsReady()) {
    impl_->GetClientOverview(type, std::move(callback));
    return;
  }
  MaybeCacheClosure(base::BindOnce(
      &InitAwareNotificationScheduler::GetClientOverview,
      weak_ptr_factory_.GetWeakPtr(), type, std::move(callback)));
}

void InitAwareNotificationScheduler::OnStartTask(
    TaskFinishedCallback callback) {
  if (IsReady()) {
    impl_->OnStartTask(std::move(callback));
    return;
  }
  MaybeCacheClosure(base::BindOnce(&InitAwareNotificationScheduler::OnStartTask,
                                   weak_ptr_factory_.GetWeakPtr(),
                                   std::move(callback)));
}

void InitAwareNotificationScheduler::OnStopTask() {
  if (IsReady()) {
    impl_->OnStopTask();
    return;
  }
  MaybeCacheClosure(base::BindOnce(&InitAwareNotificationScheduler::OnStopTask,
                                   weak_ptr_factory_.GetWeakPtr()));
}

void InitAwareNotificationScheduler::OnUserAction(
    const UserActionData& action_data) {
  if (IsReady()) {
    impl_->OnUserAction(action_data);
    return;
  }
  MaybeCacheClosure(
      base::BindOnce(&InitAwareNotificationScheduler::OnUserAction,
                     weak_ptr_factory_.GetWeakPtr(), action_data));
}

void InitAwareNotificationScheduler::OnInitialized(InitCallback init_callback,
                                                   bool success) {
  init_success_ = success;
  if (!success) {
    cached_closures_.clear();
    std::move(init_callback).Run(false);
    return;
  }

  // Flush all cached calls.
  for (auto it = cached_closures_.begin(); it != cached_closures_.end(); ++it) {
    std::move(*it).Run();
  }
  cached_closures_.clear();
  std::move(init_callback).Run(true);
}

bool InitAwareNotificationScheduler::IsReady() const {
  return init_success_.has_value() && *init_success_;
}

void InitAwareNotificationScheduler::MaybeCacheClosure(
    base::OnceClosure closure) {
  DCHECK(closure);

  // Drop the call if initialization failed.
  if (init_success_.has_value() && !*init_success_)
    return;

  // Cache the closure to invoke later.
  cached_closures_.emplace_back(std::move(closure));
}

}  // namespace notifications
