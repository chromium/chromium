// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/ml/boot_clock.h"

#include <time.h>

#include "base/logging.h"
#include "base/time/time_override.h"

namespace chromeos {
namespace power {
namespace ml {

BootClock::BootClock()
    : mock_boot_time_(base::subtle::ScopedTimeClockOverrides::overrides_active()
                          ? base::TimeTicks::Now() -
                                base::TimeDelta::FromMinutes(5)
                          : base::TimeTicks()) {}

BootClock::~BootClock() = default;

base::TimeDelta BootClock::GetTimeSinceBoot() const {
  DCHECK_EQ(base::subtle::ScopedTimeClockOverrides::overrides_active(),
            !mock_boot_time_.is_null())
      << "Time overrides must not change during BootClock's lifetime.";
  if (!mock_boot_time_.is_null())
    return base::TimeTicks::Now() - mock_boot_time_;

  struct timespec ts = {0};
  const int ret = clock_gettime(CLOCK_BOOTTIME, &ts);
  DCHECK_EQ(ret, 0);
  return base::TimeDelta::FromTimeSpec(ts);
}

}  // namespace ml
}  // namespace power
}  // namespace chromeos
