// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_UTIL_MEMORY_PRESSURE_MEMORY_PRESSURE_LEVEL_REPORTER_H_
#define BASE_UTIL_MEMORY_PRESSURE_MEMORY_PRESSURE_LEVEL_REPORTER_H_

#include <array>

#include "base/memory/memory_pressure_listener.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace util {

// Report metrics related to memory pressure.
class MemoryPressureLevelReporter {
 public:
  using MemoryPressureLevel = base::MemoryPressureListener::MemoryPressureLevel;

  explicit MemoryPressureLevelReporter(
      MemoryPressureLevel initial_pressure_level);
  ~MemoryPressureLevelReporter();

  // Should be called whenever the current memory pressure level changes.
  void OnMemoryPressureLevelChanged(MemoryPressureLevel new_level);

 private:
  void ReportHistogram(base::TimeTicks now);
  void StartPeriodicTimer();

  MemoryPressureLevel current_pressure_level_;
  base::TimeTicks current_pressure_level_begin_ = base::TimeTicks::Now();

  // The reporting of the pressure level histogram is done in seconds, the
  // duration in a given pressure state will be floored. This means that some
  // time will be truncated each time we send a report. This array is used to
  // accumulate the truncated time and add it to the reported value when it
  // exceeds one second.
  std::array<base::TimeDelta, MemoryPressureLevel::kMaxValue + 1>
      accumulator_buckets_;

  // Timer used to ensure a periodic reporting of the pressure level metric.
  // Without this there's a risk that a browser crash will cause some data to
  // be lost.
  base::OneShotTimer periodic_reporting_timer_;
};

}  // namespace util

#endif  // BASE_UTIL_MEMORY_PRESSURE_MEMORY_PRESSURE_LEVEL_REPORTER_H_
