// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/notification_background_task_scheduler_impl.h"

#include "base/logging.h"

NotificationBackgroundTaskSchedulerImpl::
    NotificationBackgroundTaskSchedulerImpl() = default;

NotificationBackgroundTaskSchedulerImpl::
    ~NotificationBackgroundTaskSchedulerImpl() = default;

void NotificationBackgroundTaskSchedulerImpl::Schedule(
    base::TimeDelta window_start,
    base::TimeDelta window_end) {
  // TODO(xingliu): Implements this for non-Android platform.
  NOTIMPLEMENTED();
}

void NotificationBackgroundTaskSchedulerImpl::Cancel() {
  NOTIMPLEMENTED();
}
