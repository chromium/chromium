// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_UTIL_H_
#define ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_UTIL_H_

#include <string>

#include "ash/ash_export.h"
#include "base/time/time.h"

namespace ash {

class SystemTextfield;

namespace focus_mode_util {

constexpr base::TimeDelta kMinimumDuration = base::Minutes(1);
constexpr base::TimeDelta kMaximumDuration = base::Minutes(300);

enum class TimeFormatType {
  // Times formatted with `kDigital` type include seconds, minutes, and hours
  // in a numeric width, without removing leading zeros. Used in the labels
  // under the progress bar in the countdown view, for example "4:30".
  kDigital,
  // Times formatted with `kFull` type include seconds, minutes, and hours in
  // short width. Used in the time remaining timer in the countdown view, for
  // example "4 min, 30 sec".
  kFull,
  // Times formatted with `kMinutesOnly` type include hours only when focus is
  // active, and minutes in a short width. Used in the time remaining display
  // in the detailed view, for example "4 min".
  kMinutesOnly,
};

// Adaptation of `base::TimeDurationFormat`. This helper function
// takes a `TimeDelta` and returns the time formatted according to
// `format_type`. See `TimeFormatType` for more details.
// `TimeFormatType::kDigital` removes the hour if it is a leading zero. For
// example "0:30", "4:30", "1:04:30".
// `TimeFormatType::kFull` removes the hour if it is a leading zero and the
// minute if it is a leading zero. For example "30 sec", "4 min, 30 sec", "1
// hr, 4 min, 30 sec". When focus is active:
//   `TimeFormatType::kMinutesOnly` removes the seconds and removes the hour
//   when it is a leading zero. For example "0 min", "4 min", "1 hr, 4 min".
// When focus is not active:
//   `TimeFormatType::kMinutesOnly` removes the seconds and the hour. For
//   example "0 min", "4 min", "64 min".
// All examples were for times of 30 seconds, 4 minutes and 30 seconds, and 1
// hour 4 minutes and 30 seconds.
// Returns a default formatted string in cases where formatting the time
// duration returns an error.
ASH_EXPORT std::u16string GetDurationString(base::TimeDelta duration_to_format,
                                            TimeFormatType format_type);

// Returns a string of `end_time` formatted with the correct clock type. For
// example: "5:10 PM" for 12-hour clock, "17:10" for 24-hour clock.
ASH_EXPORT std::u16string GetFormattedClockString(const base::Time end_time);

// Returns a string indicating that do not disturb will be turned off when the
// focus session ends at `end_time`.
ASH_EXPORT std::u16string GetNotificationTitleForFocusSession(
    const base::Time end_time);

// Reads the `timer_textfield`'s text and converts it to an integer.
ASH_EXPORT int GetTimerTextfieldInputInMinutes(
    SystemTextfield* timer_textfield);

}  // namespace focus_mode_util

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_UTIL_H_
