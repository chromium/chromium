// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HUD_DISPLAY_DATA_SOURCE_H_
#define ASH_HUD_DISPLAY_DATA_SOURCE_H_

#include <cstdint>
#include <limits>
#include "ash/hud_display/cpu_stats.h"

namespace ash {
namespace hud_display {

// This is source of data to draw the HUD display.
class DataSource {
 public:
  struct Snapshot {
    Snapshot();
    Snapshot(const Snapshot&);
    Snapshot& operator=(const Snapshot&);

    // All memory sizes are in bytes.
    // Separate non-zero-initialized members from zero-initialized.
    int64_t free_ram{std::numeric_limits<int64_t>::max()};

    int64_t physical_ram = 0;  // Amount of physical RAM installed.
    int64_t total_ram = 0;     // As reported in /proc/meminfo.

    // Amount of RSS Private memory used by "/android" cgroup.
    int64_t arc_rss = 0;
    // Amount of RSS Shared memory used by "/android" cgroup.
    int64_t arc_rss_shared = 0;
    // Amount of RSS Private memory used by Chrome browser process.
    int64_t browser_rss = 0;
    // Amount of RSS Shared memory used by Chrome browser process.
    int64_t browser_rss_shared = 0;
    // Amount of GPU memory used by kernel GPU driver.
    int64_t gpu_kernel = 0;
    // Amount of RSS Private memory used by Chrome GPU process.
    int64_t gpu_rss = 0;
    // Amount of RSS Shared memory used by Chrome GPU process.
    int64_t gpu_rss_shared = 0;
    // Amount of RSS Private memory used by Chrome type=renderer processes.
    int64_t renderers_rss = 0;
    // Amount of RSS Shared memory used by Chrome type=renderer processes.
    int64_t renderers_rss_shared = 0;

    // CPU stats are calculated only in GetSnapshotAndReset().
    // CPU usage values should sum to 1.
    float cpu_idle_part = 0;    // Amount spent in idle state.
    float cpu_user_part = 0;    // Amount spent in user + nice mode.
    float cpu_system_part = 0;  // Amount spent in system mode.
    float cpu_other_part = 0;   // Other states: irq, etc.
  };

  DataSource();
  DataSource(const DataSource&) = delete;
  DataSource& operator=(const DataSource&) = delete;
  ~DataSource();

  // This must be called on io-enabled thread.
  Snapshot GetSnapshotAndReset();

 private:
  void Refresh();

  Snapshot GetSnapshot() const;
  void ResetCounters();

  // Current system snapshot.
  Snapshot snapshot_;

  // Last CPU stats snapshot.
  CpuStats cpu_stats_latest_;

  // Last stats before Reset() to calculate delta.
  CpuStats cpu_stats_base_;
};

}  // namespace hud_display
}  // namespace ash

#endif  // ASH_HUD_DISPLAY_DATA_SOURCE_H_
