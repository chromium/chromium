// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/default_tick_clock.h"

#include "base/no_destructor.h"

#include "base/record_replay.h"

namespace base {

DefaultTickClock::~DefaultTickClock() = default;

TimeTicks DefaultTickClock::NowTicks() const {
  // https://linear.app/replay/issue/RUN-1145
  if (!recordreplay::AreEventsDisallowed())
    recordreplay::Assert("[RUN-1145] DefaultTickClock::NowTicks");

  return TimeTicks::Now();
}

// static
const DefaultTickClock* DefaultTickClock::GetInstance() {
  static const base::NoDestructor<DefaultTickClock> default_tick_clock;
  return default_tick_clock.get();
}

}  // namespace base
