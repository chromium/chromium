// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/policy/weekly_time/weekly_time_interval.h"

#include "base/logging.h"
#include "base/time/time.h"

namespace em = enterprise_management;

namespace policy {

// static
const char WeeklyTimeInterval::kStart[] = "start";
const char WeeklyTimeInterval::kEnd[] = "end";

WeeklyTimeInterval::WeeklyTimeInterval(const WeeklyTime& start,
                                       const WeeklyTime& end)
    : start_(start), end_(end) {
  DCHECK_GT(start.GetDurationTo(end), base::TimeDelta());
  DCHECK(start.timezone_offset() == end.timezone_offset());
}

WeeklyTimeInterval::WeeklyTimeInterval(const WeeklyTimeInterval& rhs) = default;

WeeklyTimeInterval& WeeklyTimeInterval::operator=(
    const WeeklyTimeInterval& rhs) = default;

base::Value WeeklyTimeInterval::ToValue() const {
  base::Value interval(base::Value::Type::DICTIONARY);
  interval.SetKey(kStart, start_.ToValue());
  interval.SetKey(kEnd, end_.ToValue());
  return interval;
}

bool WeeklyTimeInterval::Contains(const WeeklyTime& w) const {
  DCHECK_EQ(start_.timezone_offset().has_value(),
            w.timezone_offset().has_value());
  if (w.GetDurationTo(end_).is_zero())
    return false;
  base::TimeDelta interval_duration = start_.GetDurationTo(end_);
  return start_.GetDurationTo(w) + w.GetDurationTo(end_) == interval_duration;
}

// static
std::unique_ptr<WeeklyTimeInterval> WeeklyTimeInterval::ExtractFromProto(
    const em::WeeklyTimeIntervalProto& container,
    absl::optional<int> timezone_offset) {
  if (!container.has_start() || !container.has_end()) {
    LOG(WARNING) << "Interval without start or/and end.";
    return nullptr;
  }
  auto start = WeeklyTime::ExtractFromProto(container.start(), timezone_offset);
  auto end = WeeklyTime::ExtractFromProto(container.end(), timezone_offset);
  if (!start || !end)
    return nullptr;
  return std::make_unique<WeeklyTimeInterval>(*start, *end);
}

// static
std::unique_ptr<WeeklyTimeInterval> WeeklyTimeInterval::ExtractFromValue(
    const base::Value* value,
    absl::optional<int> timezone_offset) {
  if (!value || !value->FindDictKey(kStart) || !value->FindDictKey(kEnd)) {
    LOG(WARNING) << "Interval without start or/and end.";
    return nullptr;
  }
  auto start =
      WeeklyTime::ExtractFromValue(value->FindDictKey(kStart), timezone_offset);
  auto end =
      WeeklyTime::ExtractFromValue(value->FindDictKey(kEnd), timezone_offset);
  if (!start || !end)
    return nullptr;
  return std::make_unique<WeeklyTimeInterval>(*start, *end);
}

}  // namespace policy
