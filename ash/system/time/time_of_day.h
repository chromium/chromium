// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TIME_TIME_OF_DAY_H_
#define ASH_SYSTEM_TIME_TIME_OF_DAY_H_

#include <optional>
#include <ostream>
#include <string>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/time/clock.h"
#include "base/time/time.h"

namespace ash {

class LocalTimeConverter;

// Represents the time of the day as a simple number of minutes since 00:00
// regardless of the date or the timezone. This makes it simple to persist this
// as an integer user pref.
class ASH_EXPORT TimeOfDay {
 public:
  // |offset_minutes| is the number of minutes since 00:00. If |offset_minutes|
  // is equal to the offset minutes in 24 hours, it will be reset to 0 to
  // represent the time 00:00 (12:00 AM). Offsets greater than the minutes in
  // 24 hours are not allowed.
  explicit TimeOfDay(int offset_minutes);
  TimeOfDay(const TimeOfDay& other) = default;
  TimeOfDay& operator=(const TimeOfDay& rhs) = default;
  ~TimeOfDay() = default;

  // Converts to a minutes offset representation from |time| dropping the
  // seconds and milliseconds.
  static TimeOfDay FromTime(const base::Time& time);

  bool operator==(const TimeOfDay& rhs) const;

  int offset_minutes_from_zero_hour() const {
    return offset_minutes_from_zero_hour_;
  }

  // Sets `clock_` with a given `clock`, but this class does not own it.
  // The clock is used to determine current time in `GetNow()`.
  TimeOfDay& SetClock(const base::Clock* clock);

  // Sets a custom `LocalTimeConverter` for tests to simulate failed local time
  // operations (i.e. `ToTimeToday()` returns a null `base::Time`). May be
  // called multiple times, or set to `nullptr` for the default local time
  // implementations to be used.
  TimeOfDay& SetLocalTimeConverter(
      const LocalTimeConverter* local_time_converter);

  // Converts to an actual point in time today. If this fails for some reason,
  // `std::nullopt` will be returned. Otherwise, always returns a valid
  // non-null timestamp.
  std::optional<base::Time> ToTimeToday() const;

  // Converts to a string in the format "3:07 PM". Only for debugging use.
  std::string ToString() const;

 private:
  // Gets now time from the `clock_`, used for testing, or `base::Time::Now()`
  // if `clock_` does not exist.
  base::Time GetNow() const;

  const LocalTimeConverter& GetLocalTimeConverter() const;

  int offset_minutes_from_zero_hour_;

  // Optional Used in tests to override the time of "Now".
  raw_ptr<const base::Clock> clock_ = nullptr;  // Not owned.

  // May be null, in which case `GetLocalTimeConverter()` returns the default
  // implementation.
  raw_ptr<const LocalTimeConverter> local_time_converter_ = nullptr;
};

ASH_EXPORT std::ostream& operator<<(std::ostream& os,
                                    const TimeOfDay& time_of_day);

}  // namespace ash

#endif  // ASH_SYSTEM_TIME_TIME_OF_DAY_H_
