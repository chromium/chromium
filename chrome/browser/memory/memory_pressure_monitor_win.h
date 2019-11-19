// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEMORY_MEMORY_PRESSURE_MONITOR_WIN_H_
#define CHROME_BROWSER_MEMORY_MEMORY_PRESSURE_MONITOR_WIN_H_

#include "base/macros.h"
#include "base/timer/timer.h"
#include "chrome/browser/memory/memory_pressure_monitor.h"
#include "chrome/browser/memory/memory_pressure_monitor_utils.h"
#include "chrome/browser/performance_monitor/system_monitor.h"

namespace memory {

// Windows implementation of the memory pressure monitor.
//
// The global performance_monitor::SystemMonitor instance should be initialized
// before the creation of this object.
class MemoryPressureMonitorWin
    : public MemoryPressureMonitor,
      public performance_monitor::SystemMonitor::SystemObserver {
 public:
  ~MemoryPressureMonitorWin() override;

  base::MemoryPressureListener::MemoryPressureLevel
  memory_pressure_level_for_testing() const {
    return memory_pressure_level();
  }

  const FreeMemoryObservationWindow& free_memory_obs_window_for_testing()
      const {
    return free_memory_obs_window_;
  }

  const DiskIdleTimeObservationWindow& disk_idle_time_obs_window_for_testing()
      const {
    return disk_idle_time_obs_window_;
  }

 protected:
  // This object is expected to be created via MemoryPressureMonitor::Create.
  friend class MemoryPressureMonitor;
  friend class MemoryPressureMonitorWinTest;

  MemoryPressureMonitorWin();

  // performance_monitor::SystemMonitor::SystemObserver:
  void OnFreePhysicalMemoryMbSample(int free_phys_memory_mb) override;
  void OnDiskIdleTimePercent(float disk_idle_time_percent) override;

  // Should be called each time one of the observation windows gets updated,
  // this will check if the system is under memory pressure.
  void OnObservationWindowUpdate();

  // Check the observations windows and returns the current memory pressure
  // level.
  base::MemoryPressureListener::MemoryPressureLevel
  CheckObservationWindowsAndComputeLevel();

  // The free memory observation window.
  FreeMemoryObservationWindow free_memory_obs_window_;

  // The disk idle time observation window.
  DiskIdleTimeObservationWindow disk_idle_time_obs_window_;

  // The refresh frequency of the various metrics tracked by this class.
  performance_monitor::SystemMonitor::SystemObserver::MetricRefreshFrequencies
      refresh_frequencies_;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(MemoryPressureMonitorWin);
};

}  // namespace memory

#endif  // CHROME_BROWSER_MEMORY_MEMORY_PRESSURE_MONITOR_WIN_H_
