// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TIME_ASTRONOMER_UTIL_H_
#define ASH_SYSTEM_TIME_ASTRONOMER_UTIL_H_

#include "ash/ash_export.h"
#include "base/time/time.h"
#include "base/types/expected.h"

namespace ash {

enum class SunRiseSetError {
  // The current geolocation has no sunrise/sunset (24 hours of daylight or
  // darkness).
  kNoSunRiseSet,
  // Sunrise/set are temporarily unavailable, including the default values of
  // 6 AM/PM local time. Caller should handle this gracefully and try again
  // later.
  kUnavailable,
};

struct SunRiseSetTime {
  base::Time sunrise;
  base::Time sunset;
};

// Calculates the sunrise and sunset times for a given 'time` and location
// given by `latitude` and `longtitude`.
ASH_EXPORT base::expected<SunRiseSetTime, SunRiseSetError>
GetSunriseSunset(const base::Time& time, double latitude, double longitude);

}  // namespace ash

#endif  // ASH_SYSTEM_TIME_ASTRONOMER_UTIL_H_
