// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_mock_clock_override.h"

#include <ostream>

#include "base/check_op.h"

namespace base {

ScopedMockClockOverride* ScopedMockClockOverride::scoped_mock_clock_ = nullptr;

ScopedMockClockOverride::ScopedMockClockOverride()
    :  // Start the offset past zero so that it's not treated as a null value.
      offset_(Days(365)) {
  DCHECK(!scoped_mock_clock_)
      << "Nested ScopedMockClockOverrides are not supported.";

  scoped_mock_clock_ = this;

  time_clock_overrides_ = std::make_unique<subtle::ScopedTimeClockOverrides>(
      &ScopedMockClockOverride::Now, &ScopedMockClockOverride::NowTicks,
      &ScopedMockClockOverride::NowThreadTicks);
}

ScopedMockClockOverride::~ScopedMockClockOverride() {
  scoped_mock_clock_ = nullptr;
}

Time ScopedMockClockOverride::Now() {
  return Time() + scoped_mock_clock_->offset_;
}

TimeTicks ScopedMockClockOverride::NowTicks() {
  return TimeTicks() + scoped_mock_clock_->offset_;
}

ThreadTicks ScopedMockClockOverride::NowThreadTicks() {
  return ThreadTicks() + scoped_mock_clock_->offset_;
}

void ScopedMockClockOverride::Advance(TimeDelta delta) {
  DCHECK_GT(delta, base::TimeDelta())
      << "Monotonically increasing time may not go backwards";
  offset_ += delta;
}

}  // namespace base
