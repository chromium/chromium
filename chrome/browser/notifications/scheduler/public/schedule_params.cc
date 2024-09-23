// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/public/schedule_params.h"

namespace notifications {

ScheduleParams::ScheduleParams() : priority(Priority::kLow) {}

ScheduleParams::ScheduleParams(const ScheduleParams& other) = default;

ScheduleParams::ScheduleParams(ScheduleParams&& other) = default;

ScheduleParams& ScheduleParams::operator=(const ScheduleParams& other) =
    default;

ScheduleParams& ScheduleParams::operator=(ScheduleParams&& other) = default;

ScheduleParams::~ScheduleParams() = default;

bool ScheduleParams::operator==(const ScheduleParams& other) const {
  return priority == other.priority &&
         impression_mapping == other.impression_mapping &&
         deliver_time_start == other.deliver_time_start &&
         deliver_time_end == other.deliver_time_end &&
         ignore_timeout_duration == other.ignore_timeout_duration;
}

}  // namespace notifications
