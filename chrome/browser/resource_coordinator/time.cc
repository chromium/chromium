// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/time.h"

#include "base/check.h"
#include "base/logging.h"
#include "base/time/clock.h"
#include "base/time/tick_clock.h"

namespace resource_coordinator {

namespace {

const base::TickClock* g_tick_clock_for_testing = nullptr;
const base::Clock* g_clock_for_testing = nullptr;

}  // namespace

base::TimeTicks NowTicks() {
  return g_tick_clock_for_testing ? g_tick_clock_for_testing->NowTicks()
                                  : base::TimeTicks::Now();
}

base::Time Now() {
  return g_clock_for_testing ? g_clock_for_testing->Now() : base::Time::Now();
}

const base::TickClock* GetTickClock() {
  return g_tick_clock_for_testing;
}

ScopedSetClocksForTesting::ScopedSetClocksForTesting(
    const base::Clock* clock,
    const base::TickClock* tick_clock) {
  DCHECK(!g_tick_clock_for_testing);
  DCHECK(!g_clock_for_testing);

  g_tick_clock_for_testing = tick_clock;
  g_clock_for_testing = clock;
}

ScopedSetClocksForTesting::~ScopedSetClocksForTesting() {
  g_tick_clock_for_testing = nullptr;
  g_clock_for_testing = nullptr;
}

}  // namespace resource_coordinator
