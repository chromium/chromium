// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/simple_test_tick_clock.h"

#include "base/check.h"

namespace base {

SimpleTestTickClock::SimpleTestTickClock() = default;

SimpleTestTickClock::~SimpleTestTickClock() = default;

TimeTicks SimpleTestTickClock::NowTicks() const {
  AutoLock lock(lock_);
  return now_ticks_;
}

void SimpleTestTickClock::Advance(TimeDelta delta) {
  AutoLock lock(lock_);
  DCHECK(delta >= TimeDelta());
  now_ticks_ += delta;
}

void SimpleTestTickClock::SetNowTicks(TimeTicks ticks) {
  AutoLock lock(lock_);
  now_ticks_ = ticks;
}

}  // namespace base
