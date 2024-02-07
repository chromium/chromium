// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/repeating_time_interval_task_executor.h"

#include "base/functional/callback.h"

namespace ash {

RepeatingTimeIntervalTaskExecutor::RepeatingTimeIntervalTaskExecutor(
    const policy::WeeklyTimeInterval& time_interval,
    base::RepeatingClosure interval_start_callback,
    base::RepeatingClosure interval_end_callback)
    : time_interval_(time_interval),
      interval_start_callback_(interval_start_callback),
      interval_end_callback_(interval_end_callback) {}

RepeatingTimeIntervalTaskExecutor::~RepeatingTimeIntervalTaskExecutor() =
    default;

}  // namespace ash
