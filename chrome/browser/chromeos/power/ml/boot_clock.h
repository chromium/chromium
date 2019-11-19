// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_ML_BOOT_CLOCK_H_
#define CHROME_BROWSER_CHROMEOS_POWER_ML_BOOT_CLOCK_H_

#include "base/time/time.h"

namespace chromeos {
namespace power {
namespace ml {

// A class that returns time since boot. The time since boot always increases
// even when system is suspended (unlike TimeTicks -- for now and unless
// crbug.com/166153 resolves in favor of absolute ticks everywhere).
// BootClock supports
// base::test::TaskEnvironment::TimeSource::MOCK_TIME. When time is
// mocked, it will use the mocked TimeTicks::Now() to compute its delta.
class BootClock {
 public:
  BootClock();
  ~BootClock();

  base::TimeDelta GetTimeSinceBoot() const;

 private:
  // Null unless time is mocked. When time is mocked, this pretends boot
  // happened 5 minutes before the creation of this BootClock.
  const base::TimeTicks mock_boot_time_;

  DISALLOW_COPY_AND_ASSIGN(BootClock);
};

}  // namespace ml
}  // namespace power
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_ML_BOOT_CLOCK_H_
