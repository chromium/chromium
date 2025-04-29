// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <sys/time.h>
#include <time.h>

#include <limits>

#include "base/check_op.h"
#include "base/numerics/safe_math.h"
#include "base/time/time.h"

namespace base {

// static
TimeDelta TimeDelta::FromTimeSpec(const timespec& ts) {
  // TODO(crbug.com/41405098): Convert the max possible `timespec` explicitly to
  // `TimeDelta::Max()`, similar to `Time::FromTimeVal`.

  const TimeDelta delta = Seconds(ts.tv_sec) + Nanoseconds(ts.tv_nsec);
  return delta.is_positive() ? delta : TimeDelta();
}

struct timespec TimeDelta::ToTimeSpec() const {
  if (is_negative()) {
    return {
        .tv_sec = 0,
        .tv_nsec = 0,
    };
  }

  // TODO(crbug.com/41405098): If `time_t` is 32-bit, out of range values should
  // be converted to the max possible `timespec`, specifically with `tv_nsec =
  // kNanosecondsPerSecond-1`.

  const int64_t extra_microseconds =
      InMicroseconds() % Time::kMicrosecondsPerSecond;
  return {
      .tv_sec = saturated_cast<time_t>(InSeconds()),
      .tv_nsec = saturated_cast<long>(extra_microseconds *
                                      Time::kNanosecondsPerMicrosecond),
  };
}

// static
Time Time::FromTimeVal(struct timeval t) {
  DCHECK_LT(t.tv_usec, static_cast<int>(Time::kMicrosecondsPerSecond));
  DCHECK_GE(t.tv_usec, 0);
  if (t.tv_usec == 0 && t.tv_sec == 0) {
    return Time();
  }
  if (t.tv_usec == static_cast<suseconds_t>(Time::kMicrosecondsPerSecond) - 1 &&
      t.tv_sec == std::numeric_limits<time_t>::max()) {
    return Max();
  }
  return Time((static_cast<int64_t>(t.tv_sec) * Time::kMicrosecondsPerSecond) +
              t.tv_usec + kTimeTToMicrosecondsOffset);
}

struct timeval Time::ToTimeVal() const {
  struct timeval result;
  if (is_null()) {
    result.tv_sec = 0;
    result.tv_usec = 0;
    return result;
  }
  if (is_max()) {
    result.tv_sec = std::numeric_limits<time_t>::max();
    result.tv_usec = static_cast<suseconds_t>(Time::kMicrosecondsPerSecond) - 1;
    return result;
  }
  int64_t us = us_ - kTimeTToMicrosecondsOffset;
  result.tv_sec = static_cast<time_t>(us / Time::kMicrosecondsPerSecond);
  result.tv_usec = us % Time::kMicrosecondsPerSecond;
  return result;
}

}  // namespace base
