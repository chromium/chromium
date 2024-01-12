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
constexpr base::TimeDelta kEndingMomentDuration = base::Seconds(6);

// The amount of time to extend the focus session duration by during a currently
// active focus session.
constexpr base::TimeDelta kExtendDuration = base::Minutes(10);

// Adaptation of `base::TimeDurationFormat`. This helper function
// takes a `TimeDelta` and returns the time formatted according to
// `digital_format`.
// Passing `true` for `digital_format` returns `duration_to_format` in a numeric
//   width, excluding the hour if it is a leading zero. For example "0:30",
//   "4:30", "1:04:30".
// Passing `false` for `digital_format` returns the time in a short width
//   including hours only when nonzero and focus mode is active, minutes when
//   not a leading zero, and seconds only when `duration_to_format` is less than
//   a minute. For example when focus mode is active "30 sec", "4 min", "1 hr, 4
//   min", and when focus mode is not active "30 sec", "4 min", "64 min".
// All examples were for times of 30 seconds, 4 minutes and 30 seconds, and 1
// hour 4 minutes and 30 seconds.
// Returns a default formatted string in cases where formatting the time
// duration returns an error.
ASH_EXPORT std::u16string GetDurationString(base::TimeDelta duration_to_format,
                                            bool digital_format);

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

// Returns a string of `end_time` formatted for the "Until" end time label. For
// example: "Until 1:00 PM".
ASH_EXPORT std::u16string GetFormattedEndTimeString(const base::Time end_time);

}  // namespace focus_mode_util

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_UTIL_H_
