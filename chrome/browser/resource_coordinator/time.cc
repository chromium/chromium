// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/time.h"

#include "base/check.h"
#include "base/time/tick_clock.h"

namespace resource_coordinator {

namespace {

const base::TickClock* g_tick_clock_for_testing = nullptr;

}  // namespace

base::TimeTicks NowTicks() {
  return g_tick_clock_for_testing ? g_tick_clock_for_testing->NowTicks()
                                  : base::TimeTicks::Now();
}

const base::TickClock* GetTickClock() {
  return g_tick_clock_for_testing;
}

ScopedSetTickClockForTesting::ScopedSetTickClockForTesting(
    const base::TickClock* tick_clock) {
  DCHECK(!g_tick_clock_for_testing);
  g_tick_clock_for_testing = tick_clock;
}

ScopedSetTickClockForTesting::~ScopedSetTickClockForTesting() {
  g_tick_clock_for_testing = nullptr;
}

}  // namespace resource_coordinator
