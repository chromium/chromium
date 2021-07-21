// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_SCHEDULED_TASK_HANDLER_SCHEDULED_TASK_UTIL_H_
#define CHROME_BROWSER_ASH_POLICY_SCHEDULED_TASK_HANDLER_SCHEDULED_TASK_UTIL_H_

#include <string>

#include "base/values.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/scheduled_task_executor.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace policy {

// Utility methods for scheduled device policies.
namespace scheduled_task_util {

// Parses |value| into a |ScheduledTaskData|. Returns nullopt if there
// is any error while parsing |value|.
//
// It is expected that the value has the following fields:
// |task_time_field_name| - time of the day when the task should occur. The name
// of the field is passed as an argument to ParseScheduledTask method.
// |frequency| - frequency of reccurring task. Can be daily, weekly or monthly.
// |day_of_week| - optional field, used for policies that recurr weekly.
// |day_of_month| - optional field, used for policies that recurr monthly.
absl::optional<ScheduledTaskExecutor::ScheduledTaskData> ParseScheduledTask(
    const base::Value& value,
    const std::string& task_time_field_name);

}  // namespace scheduled_task_util

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_SCHEDULED_TASK_HANDLER_SCHEDULED_TASK_UTIL_H_
