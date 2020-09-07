// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEMORY_MEMORY_KILLS_MONITOR_H_
#define CHROME_BROWSER_MEMORY_MEMORY_KILLS_MONITOR_H_

#include <memory>
#include <string>

#include "base/files/scoped_file.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/atomic_flag.h"
#include "base/synchronization/lock.h"
#include "base/threading/simple_thread.h"
#include "base/time/time.h"
#include "chromeos/login/login_state/login_state.h"

namespace memory {

// Traces kernel OOM kill events and Low memory kill events (by Chrome
// TabManager). It starts logging when a user has logged in and stopped until
// the chrome process has ended (usually because of a user log out). Thus it can
// be deemed as a per user session logger.
//
// For OOM kill events, it listens to kernel message (/dev/kmsg) in a blocking
// manner. It runs in a non-joinable thread in order to avoid blocking shutdown.
// There should be only one MemoryKillsMonitor instance globally at any given
// time, otherwise UMA would receive duplicate events.
//
// For Low memory kills events, chrome calls the single global instance of
// MemoryKillsMonitor synchronously. Note that it would be from a browser thread
// other than the listening thread.
class MemoryKillsMonitor : public base::DelegateSimpleThread::Delegate,
                           public chromeos::LoginState::Observer {
 public:
  class Handle {
   public:
    // Constructs a handle that will flag |outer| as shutting down on
    // destruction.
    explicit Handle(MemoryKillsMonitor* outer);

    ~Handle();

   private:
    MemoryKillsMonitor* const outer_;

    DISALLOW_COPY_AND_ASSIGN(Handle);
  };

  MemoryKillsMonitor();
  ~MemoryKillsMonitor() override;

  // Initializes the global instance, but do not start monitoring until user
  // log in. The caller is responsible for deleting the returned handle to
  // indicate the end of monitoring.
  static std::unique_ptr<Handle> Initialize();

  // A convenient function to log a low memory kill event. It only logs events
  // after StartMonitoring() has been called.
  static void LogLowMemoryKill(const std::string& type, int estimated_freed_kb);

  // Gets the global instance for unit test.
  static MemoryKillsMonitor* GetForTesting();

 private:
  FRIEND_TEST_ALL_PREFIXES(MemoryKillsMonitorTest, TestHistograms);

  // Try to match a line in kernel message which reports OOM.
  static void TryMatchOomKillLine(const std::string& line);

  // Overridden from base::DelegateSimpleThread::Delegate:
  void Run() override;

  // LoginState::Observer overrides.
  void LoggedInStateChanged() override;

  // Starts a non-joinable thread to monitor OOM kills. This must only
  // be invoked once per process.
  void StartMonitoring();

  // Logs low memory kill event.
  void LogLowMemoryKillImpl(const std::string& type, int estimated_freed_kb);

  // Logs OOM kill event.
  void LogOOMKill();

  // A flag set when StartMonitoring() is called to indicate that monitoring has
  // been started.
  base::AtomicFlag monitoring_started_;

  // A flag set when MemoryKillsMonitor is shutdown so that its thread can poll
  // it and attempt to wind down from that point (to avoid unnecessary work, not
  // because it blocks shutdown).
  base::AtomicFlag is_shutting_down_;

  // The underlying worker thread which is non-joinable to avoid blocking
  // shutdown.
  std::unique_ptr<base::DelegateSimpleThread> non_joinable_worker_thread_;

  // The last time a low memory kill happens. Accessed from UI thread only.
  base::Time last_low_memory_kill_time_;
  // The number of low memory kills since monitoring is started. Accessed from
  // UI thread only.
  int low_memory_kills_count_ = 0;

  // The last time an OOM kill happens. Accessed from
  // |non_joinable_worker_thread_| only.
  int64_t last_oom_kill_time_ = -1;
  // The number of OOM kills since monitoring is started. Accessed from
  // |non_joinable_worker_thread_| only.
  int oom_kills_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(MemoryKillsMonitor);
};

}  // namespace memory

#endif  // CHROME_BROWSER_MEMORY_MEMORY_KILLS_MONITOR_H_
