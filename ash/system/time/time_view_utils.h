// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TIME_TIME_VIEW_UTILS_H_
#define ASH_SYSTEM_TIME_TIME_VIEW_UTILS_H_

namespace base {
class Time;
class TimeDelta;
}  // namespace base

namespace ash::time_view_utils {

// Calculates how many seconds are between `time` and the next required clock
// update.
base::TimeDelta GetTimeRemainingToNextMinute(const base::Time& time);

}  // namespace ash::time_view_utils

#endif  // ASH_SYSTEM_TIME_TIME_VIEW_UTILS_H_
