// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEMORY_MEMORY_KILLS_MONITOR_H_
#define CHROME_BROWSER_MEMORY_MEMORY_KILLS_MONITOR_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/synchronization/atomic_flag.h"
#include "base/time/time.h"
#include "chromeos/ash/components/login/login_state/login_state.h"

namespace memory {

// Traces kernel OOM kill events and Low memory kill events (by Chrome
// TabManager). It starts logging when a user has logged in and stopped until
// the chrome process has ended (usually because of a user log out). Thus it can
// be deemed as a per user session logger.
//
// For Low memory kills events, chrome calls the single global instance of
// MemoryKillsMonitor synchronously. Note that it must be called on the browser
// UI thread.
class MemoryKillsMonitor : public ash::LoginState::Observer {
 public:
  MemoryKillsMonitor();

  MemoryKillsMonitor(const MemoryKillsMonitor&) = delete;
  MemoryKillsMonitor& operator=(const MemoryKillsMonitor&) = delete;

  ~MemoryKillsMonitor() override;

  // Initializes the global instance, but do not start monitoring until user
  // log in.
  static void Initialize();

  // A convenient function to log a low memory kill event. It only logs events
  // after StartMonitoring() has been called.
  static void LogLowMemoryKill(const std::string& type, int estimated_freed_kb);

 private:
  FRIEND_TEST_ALL_PREFIXES(MemoryKillsMonitorTest, TestHistograms);

  // Gets the global instance for unit test.
  static MemoryKillsMonitor* GetForTesting();

  // LoginState::Observer overrides.
  void LoggedInStateChanged() override;

  // Starts monitoring.
  void StartMonitoring();

  // Logs low memory kill event.
  void LogLowMemoryKillImpl(const std::string& type, int estimated_freed_kb);

  // A flag set when StartMonitoring() is called to indicate that monitoring has
  // been started.
  base::AtomicFlag monitoring_started_;

  // The number of low memory kills since monitoring is started. Accessed from
  // UI thread only.
  int low_memory_kills_count_ = 0;
};

}  // namespace memory

#endif  // CHROME_BROWSER_MEMORY_MEMORY_KILLS_MONITOR_H_
