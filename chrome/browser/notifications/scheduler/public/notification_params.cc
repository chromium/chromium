// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/public/notification_params.h"

#include <utility>

#include "base/guid.h"
#include "chrome/browser/notifications/scheduler/public/schedule_params.h"

namespace notifications {

NotificationParams::NotificationParams(SchedulerClientType type,
                                       NotificationData notification_data,
                                       ScheduleParams schedule_params)
    : type(type),
      guid(base::GenerateGUID()),
      enable_ihnr_buttons(false),
      notification_data(std::move(notification_data)),
      schedule_params(std::move(schedule_params)) {}

NotificationParams::~NotificationParams() = default;

}  // namespace notifications
