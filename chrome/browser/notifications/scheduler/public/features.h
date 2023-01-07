// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_FEATURES_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_FEATURES_H_

#include "base/feature_list.h"

namespace notifications {
namespace features {

// The feature to enable NotificationScheduleService.
BASE_DECLARE_FEATURE(kNotificationScheduleService);

}  // namespace features

namespace switches {

// The switch to immediately run notification scheduler background task to show
// notification.
extern const char kNotificationSchedulerImmediateBackgroundTask[];

}  // namespace switches

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_FEATURES_H_
