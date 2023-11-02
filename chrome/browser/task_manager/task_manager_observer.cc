// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/task_manager_observer.h"

#include "chrome/browser/task_manager/task_manager_interface.h"

namespace task_manager {

// static
bool TaskManagerObserver::IsResourceRefreshEnabled(RefreshType refresh_type,
                                                   int refresh_flags) {
  return (refresh_flags & refresh_type) != 0;
}

TaskManagerObserver::TaskManagerObserver(base::TimeDelta refresh_time,
                                         int64_t resources_flags)
    : observed_task_manager_(nullptr),
      desired_refresh_time_(refresh_time < base::Seconds(1) ? base::Seconds(1)
                                                            : refresh_time),
      desired_resources_flags_(resources_flags) {}

TaskManagerObserver::~TaskManagerObserver() {
  if (observed_task_manager())
    observed_task_manager()->RemoveObserver(this);
}

void TaskManagerObserver::AddRefreshType(RefreshType type) {
  desired_resources_flags_ |= type;

  if (observed_task_manager_)
    observed_task_manager_->RecalculateRefreshFlags();
}

void TaskManagerObserver::RemoveRefreshType(RefreshType type) {
  desired_resources_flags_ &= ~type;

  if (observed_task_manager_)
    observed_task_manager_->RecalculateRefreshFlags();
}

void TaskManagerObserver::SetRefreshTypesFlags(int64_t flags) {
  desired_resources_flags_ = flags;

  if (observed_task_manager_)
    observed_task_manager_->RecalculateRefreshFlags();
}

}  // namespace task_manager
