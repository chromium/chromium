// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"

namespace base {

// static
TimeTicks TimeTicks::FromUptimeMillis(int64_t uptime_millis_value) {
  // The implementation of the SystemClock.uptimeMillis() in AOSP uses the same
  // clock as base::TimeTicks::Now(): clock_gettime(CLOCK_MONOTONIC), see in
  // platform/system/code:
  // 1. libutils/SystemClock.cpp
  // 2. libutils/Timers.cpp
  //
  // We are not aware of any motivations for Android OEMs to modify the AOSP
  // implementation of either uptimeMillis() or clock_gettime(CLOCK_MONOTONIC),
  // so we assume that there are no such customizations.
  //
  // Under these assumptions the conversion is as safe as copying the value of
  // base::TimeTicks::Now() with a loss of sub-millisecond precision.
  return TimeTicks(uptime_millis_value * Time::kMicrosecondsPerMillisecond);
}

// This file is included on chromeos_ash because it needs to interpret
// UptimeMillis values from the Android container.
#if BUILDFLAG(IS_ANDROID)

// static
TimeTicks TimeTicks::FromJavaNanoTime(int64_t nano_time_value) {
  // The implementation of the System.nanoTime() in AOSP uses the same
  // clock as UptimeMillis() and base::TimeTicks::Now():
  // clock_gettime(CLOCK_MONOTONIC), see ojluni/src/main/native/System.c in
  // AOSP.
  //
  // From Android documentation on android.os.SystemClock:
  //   [uptimeMillis()] is the basis for most interval timing such as
  //   Thread.sleep(millls), Object.wait(millis), and System.nanoTime().
  //
  // We are not aware of any motivations for Android OEMs to modify the AOSP
  // implementation of either uptimeMillis(), nanoTime, or
  // clock_gettime(CLOCK_MONOTONIC), so we assume that there are no such
  // customizations.
  //
  // Under these assumptions the conversion is as safe as copying the value of
  // base::TimeTicks::Now() without the (theoretical) sub-microsecond
  // resolution.
  return TimeTicks(nano_time_value / Time::kNanosecondsPerMicrosecond);
}

jlong TimeTicks::ToUptimeMillis() const {
  // See FromUptimeMillis. UptimeMillis and TimeTicks use the same clock source,
  // and only differ in resolution.
  return us_ / Time::kMicrosecondsPerMillisecond;
}

jlong TimeTicks::ToUptimeMicros() const {
  // Same as ToUptimeMillis but maintains sub-millisecond precision.
  return us_;
}

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace base
