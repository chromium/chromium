// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEMORY_MEMORY_PRESSURE_MONITOR_H_
#define CHROME_BROWSER_MEMORY_MEMORY_PRESSURE_MONITOR_H_

#include <memory>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"

namespace features {
extern const base::Feature kNewMemoryPressureMonitor;
}

namespace memory {

// The memory pressure monitor is responsible for monitoring some system metrics
// and determining when the system is under memory pressure.
//
// Each platform interested in tracking this should provide a platform specific
// implementation and make MemoryPressureMonitor::Create return it. These
// implementation are responsible for tracking the metrics they need (i.e. if
// some things need to run on a timer these classes should own this timer), they
// also need to call |OnMemoryPressureLevelChange| when the memory pressure
// level changes.
//
// It is recommended to use performance_monitor::SystemMonitor for the platform
// specific implementation.
//
// A single instance of this class can exist at the same time, in practice it is
// expected to be owned by the browser process.
//
// This class isn't thread safe, it should be created and used on the same
// sequence.
//
// NOTE: This class is still a work in progress and doesn't do anything yet.
class MemoryPressureMonitor {
 public:
  virtual ~MemoryPressureMonitor();

  // Create the global instance.
  static std::unique_ptr<MemoryPressureMonitor> Create();

 protected:
  MemoryPressureMonitor();

  // This needs to be called by the platform specific implementation when the
  // pressure level changes.
  void OnMemoryPressureLevelChange(
      const base::MemoryPressureListener::MemoryPressureLevel new_level);

  base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level()
      const {
    return memory_pressure_level_;
  }

 private:
  // The last observed memory pressure level. Updated by
  // |OnMemoryPressureLevelChange|.
  base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level_ =
      base::MemoryPressureListener::MemoryPressureLevel::
          MEMORY_PRESSURE_LEVEL_NONE;

  // The latest pressure level change, used to record metrics.
  base::TimeTicks latest_level_change_ = base::TimeTicks::Now();

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(MemoryPressureMonitor);
};

}  // namespace memory

#endif  // CHROME_BROWSER_MEMORY_MEMORY_PRESSURE_MONITOR_H_
