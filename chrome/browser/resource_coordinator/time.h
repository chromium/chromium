// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TIME_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TIME_H_

#include "base/time/time.h"

namespace base {
class TickClock;
}

namespace resource_coordinator {

// Returns the current time. Obtained from the testing TickClock if set; from
// TimeTicks::Now() otherwise.
base::TimeTicks NowTicks();

// Returns the testing TickClock.
const base::TickClock* GetTickClock();

// Sets the testing TickClock within its scope.
class ScopedSetTickClockForTesting {
 public:
  explicit ScopedSetTickClockForTesting(const base::TickClock* tick_clock);

  ScopedSetTickClockForTesting(const ScopedSetTickClockForTesting&) = delete;
  ScopedSetTickClockForTesting& operator=(const ScopedSetTickClockForTesting&) =
      delete;

  ~ScopedSetTickClockForTesting();
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TIME_H_
