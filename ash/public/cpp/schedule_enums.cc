// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ostream>

#include "ash/public/cpp/schedule_enums.h"

namespace ash {

std::ostream& operator<<(std::ostream& os, ScheduleCheckpoint checkpoint) {
  switch (checkpoint) {
    case ash::ScheduleCheckpoint::kDisabled:
      os << "kDisabled";
      break;
    case ash::ScheduleCheckpoint::kEnabled:
      os << "kEnabled";
      break;
    case ash::ScheduleCheckpoint::kLateAfternoon:
      os << "kLateAfternoon";
      break;
    case ash::ScheduleCheckpoint::kMorning:
      os << "kMorning";
      break;
    case ash::ScheduleCheckpoint::kSunrise:
      os << "kSunrise";
      break;
    case ash::ScheduleCheckpoint::kSunset:
      os << "kSunset";
      break;
  }
  return os;
}

}  // namespace ash
