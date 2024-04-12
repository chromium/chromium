// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/kill.h"

#include "base/task/thread_pool.h"

namespace base {

void EnsureProcessTerminated(Process process) {
  DCHECK(!process.is_current());

  constexpr int kWaitBeforeKillSeconds = 2;

#if TARGET_OS_SIMULATOR
  // For iOS we support "content processes", processes that are launched using
  // the BrowserEngineKit APIs (which have well defined roles and sandbox
  // restricitons). For iOS simulator we additionally support processes that are
  // forked so we can run tests (via gtest) in parallel.
  if (!process.IsContentProcess()) {
    WaitForChildToDie(process.Pid(), kWaitBeforeKillSeconds);
    return;
  }
#endif

  if (process.WaitForExitWithTimeout(TimeDelta(), nullptr)) {
    return;
  }

  ThreadPool::PostDelayedTask(
      FROM_HERE,
      {TaskPriority::BEST_EFFORT, TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      BindOnce(
          [](Process process) {
            if (process.WaitForExitWithTimeout(TimeDelta(), nullptr)) {
              return;
            }
            process.Terminate(-1, false);
          },
          std::move(process)),
      Seconds(kWaitBeforeKillSeconds));
}

}  // namespace base
