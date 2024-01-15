// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_METRICS_SHUTDOWN_WATCHER_HELPER_H_
#define CHROME_BROWSER_METRICS_SHUTDOWN_WATCHER_HELPER_H_

#include <optional>

#include "base/threading/thread_checker.h"
#include "base/threading/watchdog.h"
#include "base/time/time.h"
#include "build/build_config.h"

// ShutdownWatcherHelper is useless on Android because there is no shutdown,
// Chrome is always killed one way or another (swiped away in the task
// switcher, OOM-killed, etc.).
#if !BUILDFLAG(IS_ANDROID)
// This is a wrapper class for detecting hangs during shutdown.
class ShutdownWatcherHelper : public base::Watchdog::Delegate {
 public:
  // Create an empty holder for |shutdown_watchdog_|.
  ShutdownWatcherHelper();

  ShutdownWatcherHelper(const ShutdownWatcherHelper&) = delete;
  ShutdownWatcherHelper& operator=(const ShutdownWatcherHelper&) = delete;

  // Destructor disarm's shutdown_watchdog_ so that alarm doesn't go off.
  ~ShutdownWatcherHelper() override;

  // Spawns a thread and starts the timer. |duration| specifies how long it will
  // wait before it calls alarm.
  void Arm(const base::TimeDelta& duration);

  // base::Watchdog::Delegate implementation:
  void Alarm() override;

 private:
  std::optional<base::Watchdog> shutdown_watchdog_;
  THREAD_CHECKER(thread_checker_);
};

#endif  // !BUILDFLAG(IS_ANDROID)

#endif  // CHROME_BROWSER_METRICS_SHUTDOWN_WATCHER_HELPER_H_
