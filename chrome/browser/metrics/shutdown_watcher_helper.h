// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_METRICS_SHUTDOWN_WATCHER_HELPER_H_
#define CHROME_BROWSER_METRICS_SHUTDOWN_WATCHER_HELPER_H_

#include "base/threading/platform_thread.h"
#include "base/threading/watchdog.h"
#include "base/time/time.h"
#include "build/build_config.h"

// ShutdownWatcherHelper is useless on Android because there is no shutdown,
// Chrome is always killed one way or another (swiped away in the task
// switcher, OOM-killed, etc.).
#if !BUILDFLAG(IS_ANDROID)
// This is a wrapper class for detecting hangs during shutdown.
class ShutdownWatcherHelper {
 public:
  // Create an empty holder for |shutdown_watchdog_|.
  ShutdownWatcherHelper();

  ShutdownWatcherHelper(const ShutdownWatcherHelper&) = delete;
  ShutdownWatcherHelper& operator=(const ShutdownWatcherHelper&) = delete;

  // Destructor disarm's shutdown_watchdog_ so that alarm doesn't go off.
  ~ShutdownWatcherHelper();

  // Constructs ShutdownWatchDogThread which spawns a thread and starts timer.
  // |duration| specifies how long it will wait before it calls alarm.
  void Arm(const base::TimeDelta& duration);

  // Get the timeout after which a shutdown hang is detected, for the current
  // channel.
  static base::TimeDelta GetPerChannelTimeout(base::TimeDelta duration);

 private:
  // shutdown_watchdog_ watches for hangs during shutdown.
  base::Watchdog* shutdown_watchdog_;

  // The |thread_id_| on which this object is constructed.
  const base::PlatformThreadId thread_id_;
};

#endif  // !BUILDFLAG(IS_ANDROID)

#endif  // CHROME_BROWSER_METRICS_SHUTDOWN_WATCHER_HELPER_H_
