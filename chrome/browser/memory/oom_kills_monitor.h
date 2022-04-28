// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEMORY_OOM_KILLS_MONITOR_H_
#define CHROME_BROWSER_MEMORY_OOM_KILLS_MONITOR_H_

#include "base/no_destructor.h"
#include "base/synchronization/atomic_flag.h"
#include "base/timer/timer.h"

namespace memory {

// Traces Linux Out-of-memory killer invocation count. It checks the oom_kill
// field in /proc/vmstat periodically.
class OOMKillsMonitor {
 public:
  OOMKillsMonitor(const OOMKillsMonitor&) = delete;
  OOMKillsMonitor& operator=(const OOMKillsMonitor&) = delete;

  ~OOMKillsMonitor();

  static OOMKillsMonitor& GetInstance();

  void Initialize();

  void LogArcOOMKill(unsigned long current_oom_kills);

 private:
  FRIEND_TEST_ALL_PREFIXES(OOMKillsMonitorTest, TestHistograms);

  friend class base::NoDestructor<OOMKillsMonitor>;

  OOMKillsMonitor();

  // The number of OOM kills since monitoring is started.
  unsigned long oom_kills_count_ = 0;

  // A flag set when Initialize() is called to indicate that monitoring has
  // been started.
  bool monitoring_started_ = false;

  // The last oom kills count.
  unsigned long last_oom_kills_count_ = 0;

  // The last ARCVM OOM kills count.
  unsigned long last_arc_oom_kills_count_ = 0;

  base::RepeatingTimer checking_timer_;

  // Checks system OOM count.
  void CheckOOMKill();

  // Splits CheckOOMKill and CheckOOMKillImpl for testing.
  void CheckOOMKillImpl(unsigned long current_oom_kills);

  // Reports OOM Kills to histogram.
  void ReportOOMKills(unsigned long oom_kills_delta);
};

}  // namespace memory

#endif  // CHROME_BROWSER_MEMORY_OOM_KILLS_MONITOR_H_
