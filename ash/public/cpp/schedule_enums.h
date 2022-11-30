// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SCHEDULE_ENUMS_H_
#define ASH_PUBLIC_CPP_SCHEDULE_ENUMS_H_

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

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SCHEDULE_ENUMS_H_