// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/public/notification_params.h"

#include <utility>

#include "base/uuid.h"
#include "chrome/browser/notifications/scheduler/public/schedule_params.h"

namespace notifications {

NotificationParams::NotificationParams(SchedulerClientType type,
                                       NotificationData notification_data,
                                       ScheduleParams schedule_params)
    : type(type),
      guid(base::Uuid::GenerateRandomV4().AsLowercaseString()),
      enable_ihnr_buttons(false),
      notification_data(std::move(notification_data)),
      schedule_params(std::move(schedule_params)) {}

NotificationParams::~NotificationParams() = default;

}  // namespace notifications
