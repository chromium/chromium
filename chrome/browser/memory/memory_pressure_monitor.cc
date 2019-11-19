// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory/memory_pressure_monitor.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include "chrome/browser/memory/memory_pressure_monitor_win.h"
#endif

namespace features {

// Enables the new memory pressure monitor.
const base::Feature kNewMemoryPressureMonitor{
    "NewMemoryPressureMonitor", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features

namespace memory {
namespace {

// The global instance.
MemoryPressureMonitor* g_memory_pressure_monitor = nullptr;

void RecordPressureLevelSessionDuration(
    const base::MemoryPressureListener::MemoryPressureLevel level,
    const base::TimeDelta& session_duration) {
  switch (level) {
    case base::MemoryPressureListener::MemoryPressureLevel::
        MEMORY_PRESSURE_LEVEL_NONE:
      // Use UMA_HISTOGRAM_LONG_TIMES_100 here as it's expected that Chrome
      // will spend way more time under this level. It's logging values up
      // to one hour, which is sufficient in this case (it's not necessary
      // to have the exact length of the long sessions).
      UMA_HISTOGRAM_LONG_TIMES_100(
          "Memory.Experimental.NewMemoryPressureMonitor.SessionDurations."
          "NoPressure",
          session_duration);
      break;
    case base::MemoryPressureListener::MemoryPressureLevel::
        MEMORY_PRESSURE_LEVEL_CRITICAL:
      // The critical level sessions are expected to be short.
      UMA_HISTOGRAM_MEDIUM_TIMES(
          "Memory.Experimental.NewMemoryPressureMonitor.SessionDurations."
          "CriticalPressure",
          session_duration);
      break;
    // The moderate level isn't supported by this monitor for now.
    case base::MemoryPressureListener::MemoryPressureLevel::
        MEMORY_PRESSURE_LEVEL_MODERATE:
      NOTREACHED();
  }
}

}  // namespace

// static
std::unique_ptr<MemoryPressureMonitor> MemoryPressureMonitor::Create() {
#if defined(OS_WIN)
  return base::WrapUnique(new MemoryPressureMonitorWin());
#else
  NOTREACHED();
  return base::WrapUnique(new MemoryPressureMonitor());
#endif
}

MemoryPressureMonitor::MemoryPressureMonitor() {
  DCHECK(!g_memory_pressure_monitor);
  g_memory_pressure_monitor = this;
}

MemoryPressureMonitor::~MemoryPressureMonitor() {
  DCHECK_EQ(this, g_memory_pressure_monitor);
  g_memory_pressure_monitor = nullptr;

  RecordPressureLevelSessionDuration(
      memory_pressure_level_, latest_level_change_ - base::TimeTicks::Now());
}

void MemoryPressureMonitor::OnMemoryPressureLevelChange(
    const base::MemoryPressureListener::MemoryPressureLevel new_level) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(memory_pressure_level_, new_level);

  base::TimeTicks now = base::TimeTicks::Now();
  RecordPressureLevelSessionDuration(memory_pressure_level_,
                                     latest_level_change_ - now);
  latest_level_change_ = now;

  memory_pressure_level_ = new_level;
}

}  // namespace memory
