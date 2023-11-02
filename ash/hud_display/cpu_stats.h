// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HUD_DISPLAY_CPU_STATS_H_
#define ASH_HUD_DISPLAY_CPU_STATS_H_

#include <cstdint>

namespace ash {
namespace hud_display {

// All CPU entries from /proc/stat.
struct CpuStats {
  // These are the raw values read from /proc/stat, so as noted below,
  // their interpretation depends on the architechture. But we are using them
  // to plot relative values (0% - 100%) and thus the absolute values are not
  // important.
  // [man 5 proc]: Time, measured in units of USER_HZ (1/100ths of a second on
  // most architectures, use sysconf(_SC_CLK_TCK) to obtain the right value).
  uint64_t user;     // Time spent in user mode.
  uint64_t nice;     // Time spent in user mode with low priority (nice)
  uint64_t system;   // Time spent in system mode.
  uint64_t idle;     // Time spent in the idle task. (follows /proc/uptme)
  uint64_t iowait;   // Time waiting for I/O to complete. Not reliable.
  uint64_t irq;      // Time servicing interrupts.
  uint64_t softirq;  // Time servicing softirqs.
  uint64_t steal;    // Stolen time, which is the time spent in other operating
                     // systems when running in a virtualized environment.
  uint64_t guest;    // Time spent running a virtual CPU for guest operating
                     // systems under the control of the Linux kernel.
  uint64_t guest_nice;  // Time spent running a niced guest (virtual CPU for
                        // guest operating systems under the control of the
                        // Linux kernel).
};

// Parses current /proc/stat and restuns current values.
// Must be called on io-enabled thread.
CpuStats GetProcStatCPU();

}  // namespace hud_display
}  // namespace ash

#endif  // ASH_HUD_DISPLAY_CPU_STATS_H_
