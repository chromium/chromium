// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEMORY_MEMORY_KILLS_MONITOR_H_
#define CHROME_BROWSER_MEMORY_MEMORY_KILLS_MONITOR_H_

#include <string>

#include "base/synchronization/atomic_flag.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/login/login_state/login_state.h"

namespace memory {

// Traces kernel OOM kill events and Low memory kill events (by Chrome
// TabManager). It starts logging when a user has logged in and stopped until
// the chrome process has ended (usually because of a user log out). Thus it can
// be deemed as a per user session logger.
//
// For OOM kill events, it checks the oom_kill field in /proc/vmstat
// periodically. There should be only one MemoryKillsMonitor instance globally
// at any given time, otherwise UMA would receive duplicate events.
//
// For Low memory kills events, chrome calls the single global instance of
// MemoryKillsMonitor synchronously. Note that it must be called on the browser
// UI thread.
class MemoryKillsMonitor : public chromeos::LoginState::Observer {
 public:
  MemoryKillsMonitor();
  ~MemoryKillsMonitor() override;

  // Initializes the global instance, but do not start monitoring until user
  // log in.
  static void Initialize();

  // A convenient function to log a low memory kill event. It only logs events
  // after StartMonitoring() has been called.
  static void LogLowMemoryKill(const std::string& type, int estimated_freed_kb);

  // A convenient function to log ARCVM OOM kills.
  static void LogArcOOMKill(unsigned long current_oom_kills);

 private:
  FRIEND_TEST_ALL_PREFIXES(MemoryKillsMonitorTest, TestHistograms);

  // Gets the global instance for unit test.
  static MemoryKillsMonitor* GetForTesting();

  // LoginState::Observer overrides.
  void LoggedInStateChanged() override;

  // Starts monitoring OOM kills.
  void StartMonitoring();

  // Logs low memory kill event.
  void LogLowMemoryKillImpl(const std::string& type, int estimated_freed_kb);

  // Checks system OOM count.
  void CheckOOMKill();

  // Split CheckOOMKill and CheckOOMKillImpl for testing.
  void CheckOOMKillImpl(unsigned long current_oom_kills);

  // Logs ARCVM OOM kill.
  void LogArcOOMKillImpl(unsigned long current_oom_kills);

  // A flag set when StartMonitoring() is called to indicate that monitoring has
  // been started.
  base::AtomicFlag monitoring_started_;

  // The last time a low memory kill happens. Accessed from UI thread only.
  base::Time last_low_memory_kill_time_;
  // The number of low memory kills since monitoring is started. Accessed from
  // UI thread only.
  int low_memory_kills_count_ = 0;

  // The number of OOM kills since monitoring is started.
  unsigned long oom_kills_count_ = 0;

  // The last oom kills count from |GetCurrentOOMKills|.
  unsigned long last_oom_kills_count_ = 0;

  // The last ARCVM OOM kills count.
  unsigned long last_arc_oom_kills_count_ = 0;

  base::RepeatingTimer checking_timer_;

  DISALLOW_COPY_AND_ASSIGN(MemoryKillsMonitor);
};

}  // namespace memory

#endif  // CHROME_BROWSER_MEMORY_MEMORY_KILLS_MONITOR_H_
