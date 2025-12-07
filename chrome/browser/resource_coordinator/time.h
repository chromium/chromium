// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TIME_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TIME_H_

#include "base/time/time.h"

namespace base {
class TickClock;
class Clock;
}

namespace resource_coordinator {

// Returns the current time. Obtained from the testing TickClock if set; from
// TimeTicks::Now() otherwise.
base::TimeTicks NowTicks();

// Returns the current time. Obtained from the testing clock if set; from
// Time::Now() otherwise.
base::Time Now();

// Returns the testing TickClock.
const base::TickClock* GetTickClock();

// Returns the testing Clock.
const base::Clock* GetClock();

// Sets the testing Clock and TickClock within its scope.
class ScopedSetClocksForTesting {
 public:
  explicit ScopedSetClocksForTesting(const base::Clock* clock,
                                     const base::TickClock* tick_clock);

  ScopedSetClocksForTesting(const ScopedSetClocksForTesting&) = delete;
  ScopedSetClocksForTesting& operator=(const ScopedSetClocksForTesting&) =
      delete;

  ~ScopedSetClocksForTesting();
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TIME_H_
