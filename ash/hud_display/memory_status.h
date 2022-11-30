// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HUD_DISPLAY_MEMORY_STATUS_H_
#define ASH_HUD_DISPLAY_MEMORY_STATUS_H_

#include <cstdint>
#include <string>

#include "base/containers/flat_set.h"
#include "base/process/process_handle.h"

namespace ash {
namespace hud_display {

// Should run on the file thread.
class MemoryStatus {
 public:
  // Must be created on io-enabled thread.
  MemoryStatus();
  MemoryStatus(const MemoryStatus&) = delete;
  MemoryStatus& operator=(const MemoryStatus&) = delete;

  int64_t total_ram_size() const { return total_ram_size_; }
  int64_t total_free() const { return total_free_; }
  int64_t gpu_kernel() const { return gpu_kernel_; }

  int64_t browser_rss() const { return browser_rss_; }
  int64_t browser_rss_shared() const { return browser_rss_shared_; }

  int64_t arc_rss() const { return arc_.rss(); }
  int64_t arc_rss_shared() const { return arc_.rss_shared(); }

  int64_t gpu_rss() const { return gpu_.rss(); }
  int64_t gpu_rss_shared() const { return gpu_.rss_shared(); }

  int64_t renderers_rss() const { return renderers_.rss(); }
  int64_t renderers_rss_shared() const { return renderers_.rss_shared(); }

 private:
  // This is designed to calculate statistics for a set of processes that
  // have some command-line flag.
  // So it is initialized with command-line flag, and then TryRead()
  // is called for each potential match.
  class ProcessMemoryCountersByFlag {
   public:
    explicit ProcessMemoryCountersByFlag(const std::string& cmd_line_flag);
    ~ProcessMemoryCountersByFlag();

    ProcessMemoryCountersByFlag(const ProcessMemoryCountersByFlag&) = delete;
    ProcessMemoryCountersByFlag& operator=(const ProcessMemoryCountersByFlag&) =
        delete;

    // Returns true if |pid| belongs to the class matched by this object.
    bool TryRead(const base::ProcessId& pid, const std::string& cmdline);

    int64_t rss() const { return rss_; }
    int64_t rss_shared() const { return rss_shared_; }

   private:
    const std::string flag_;

    int64_t rss_ = 0;
    int64_t rss_shared_ = 0;
  };

  // This is designed to calculate statistics for a set of processes that
  // are within the same cgroup.
  // So it is initialized with cgroup name (and reads list of pigs in a group
  // internally).
  class ProcessMemoryCountersByCgroup {
   public:
    explicit ProcessMemoryCountersByCgroup(const std::string& expected_cgroup);
    ~ProcessMemoryCountersByCgroup();

    ProcessMemoryCountersByCgroup(const ProcessMemoryCountersByCgroup&) =
        delete;
    ProcessMemoryCountersByCgroup& operator=(
        const ProcessMemoryCountersByCgroup&) = delete;

    // Returns true if |pid| belongs to the class matched by this object.
    bool TryRead(const base::ProcessId& pid);

    int64_t rss() const { return rss_; }
    int64_t rss_shared() const { return rss_shared_; }

   private:
    base::flat_set<base::ProcessId> pids_;

    int64_t rss_ = 0;
    int64_t rss_shared_ = 0;
  };

  void UpdatePerProcessStat();

  void UpdateMeminfo();

  int64_t total_ram_size_ = 0;
  int64_t total_free_ = 0;
  int64_t browser_rss_ = 0;
  int64_t browser_rss_shared_ = 0;
  int64_t gpu_kernel_ = 0;

  ProcessMemoryCountersByFlag renderers_{"--type=renderer\0"};
  ProcessMemoryCountersByFlag gpu_{"--type=gpu-process\0"};

  ProcessMemoryCountersByCgroup arc_{"session_manager_containers/android"};
};

}  // namespace hud_display
}  // namespace ash

#endif  // ASH_HUD_DISPLAY_MEMORY_STATUS_H_
