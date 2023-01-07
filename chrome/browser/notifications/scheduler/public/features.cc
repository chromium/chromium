// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/public/features.h"

namespace notifications {
namespace features {

BASE_FEATURE(kNotificationScheduleService,
             "NotificationScheduleService",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace features

namespace switches {

const char kNotificationSchedulerImmediateBackgroundTask[] =
    "notification-scheduler-immediate-background-task";

}  // namespace switches

}  // namespace notifications
