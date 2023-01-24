// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/time_of_day.h"

#include "base/i18n/time_formatting.h"
#include "base/strings/utf_string_conversions.h"

namespace ash {

namespace {

constexpr int kMaxOffsetMinutes = 24 * 60;

}  // namespace

TimeOfDay::TimeOfDay(int offset_minutes)
    : offset_minutes_from_zero_hour_(
          offset_minutes == kMaxOffsetMinutes ? 0 : offset_minutes) {
  DCHECK_LE(offset_minutes_from_zero_hour_, kMaxOffsetMinutes);
}

// static
TimeOfDay TimeOfDay::FromTime(const base::Time& time) {
  base::Time::Exploded exploded;
  time.LocalExplode(&exploded);
  return TimeOfDay(exploded.hour * 60 + exploded.minute);
}

bool TimeOfDay::operator==(const TimeOfDay& rhs) const {
  return offset_minutes_from_zero_hour_ == rhs.offset_minutes_from_zero_hour_;
}

TimeOfDay& TimeOfDay::SetClock(const base::Clock* clock) {
  clock_ = clock;
  return *this;
}

base::Time TimeOfDay::ToTimeToday() const {
  base::Time::Exploded now;
  GetNow().LocalExplode(&now);
  now.hour = (offset_minutes_from_zero_hour_ / 60) % 24;
  now.minute = offset_minutes_from_zero_hour_ % 60;
  now.second = 0;
  now.millisecond = 0;
  base::Time result;
  if (base::Time::FromLocalExploded(now, &result))
    return result;

  // Daylight saving time can cause FromLocalExploded() to fail on the
  // transition day in the spring when TimeOfDay == 2:30 AM, and the time goes
  // instantaneously from 1:59 AM 3:00 AM. In this very rare case, it's OK for
  // this function to fail.
  return base::Time();
}

std::string TimeOfDay::ToString() const {
  return base::UTF16ToUTF8(base::TimeFormatTimeOfDay(ToTimeToday()));
}

base::Time TimeOfDay::GetNow() const {
  return clock_ ? clock_->Now() : base::Time::Now();
}

std::ostream& operator<<(std::ostream& os, const TimeOfDay& time_of_day) {
  return os << base::Minutes(time_of_day.offset_minutes_from_zero_hour());
}

}  // namespace ash
