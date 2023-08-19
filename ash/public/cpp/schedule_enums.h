// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SCHEDULE_ENUMS_H_
#define ASH_PUBLIC_CPP_SCHEDULE_ENUMS_H_

#include <ostream>

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

// These values are written to logs. New enum values can be added, but
// existing enums must never be renumbered or deleted and reused.
enum class ScheduleType {
  // Automatic toggling of ScheduledFeature is turned off.
  kNone = 0,

  // ScheduledFeature is turned automatically on at the user's local sunset
  // time, and off at the user's local sunrise time.
  kSunsetToSunrise = 1,

  // ScheduledFeature is toggled automatically based on the custom set start
  // and end times selected by the user from the system settings.
  kCustom = 2,

  // kMaxValue is required for UMA_HISTOGRAM_ENUMERATION.
  kMaxValue = kCustom,
};

// Captures all possible states that a `ScheduledFeature` can be in throughout
// the course of one day. These values are not written to logs or persisted
// anywhere.
//
// `kNone`: `kEnabled` or `kDisabled` indefinitely until the feature's "enabled"
//          pref is toggled.
//
// `kCustom`: `kEnabled` at the schedule's start time and automatically toggled
//            to `kDisabled` at the schedule's end time.
//
// `kSunsetToSunrise`:
// These checkpoints vary based on geolocation and how much daylight there is,
// but it was roughly meant to mirror the progression of a traditional day:
// Sunrise - 6 AM
// Morning (work day has started) - 10 AM
// LateAfternoon (work day is about to end) - 4 PM
// Sunset - 6 PM
//
// Formal definition: Call the total daylight time (sunset - sunrise) "D".
// * Morning: Sunrise + (D / 3)
//   * In the traditional day above, D == 12 hours and 6 AM to 10 AM == 4 hours
//     == D / 3.
// * LateAfternoon: Sunset - (D / 6)
//   * In the traditional day above, D == 12 hours and 4 PM to 6 PM == 2 hours
//     == D / 6.
enum class ScheduleCheckpoint {
  // Applies to `kNone` and `kCustom`:
  kEnabled,
  kDisabled,
  // Applies to `kSunsetToSunrise`:
  kSunset,
  kSunrise,
  kMorning,
  kLateAfternoon,
};

ASH_PUBLIC_EXPORT std::ostream& operator<<(
    std::ostream& os,
    ScheduleCheckpoint schedule_checkpoint);

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SCHEDULE_ENUMS_H_
