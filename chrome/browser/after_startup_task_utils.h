// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AFTER_STARTUP_TASK_UTILS_H_
#define CHROME_BROWSER_AFTER_STARTUP_TASK_UTILS_H_

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"

namespace android {
class AfterStartupTaskUtilsJNI;
}

namespace base {
class SequencedTaskRunner;
}

class AfterStartupTaskUtils {
 public:
  AfterStartupTaskUtils() = delete;
  AfterStartupTaskUtils(const AfterStartupTaskUtils&) = delete;
  AfterStartupTaskUtils& operator=(const AfterStartupTaskUtils&) = delete;

  // Observes startup and when complete runs tasks that have accrued.
  static void StartMonitoringStartup();

  // Queues `task` to run on `destination_runner` after startup is complete.
  // Note: prefer to simply post a task with BEST_EFFORT priority. This will
  // delay the task until higher priority tasks are finished, which includes
  // critical startup tasks. The BrowserThread::PostBestEffortTask() helper can
  // post a BEST_EFFORT task to an arbitrary task runner.
  static void PostTask(
      const base::Location& from_here,
      const scoped_refptr<base::SequencedTaskRunner>& destination_runner,
      base::OnceClosure task);

  // Returns true if browser startup is complete. Only use this on a one-off
  // basis; If you need to poll this function constantly, use the above
  // PostTask() API instead.
  static bool IsBrowserStartupComplete();

  // For use by unit tests where we don't have normal content loading
  // infrastructure and thus StartMonitoringStartup() is unsuitable.
  static void SetBrowserStartupIsCompleteForTesting();

  static void UnsafeResetForTesting();

 private:
  // TODO(wkorman): Look into why Android calls
  // SetBrowserStartupIsComplete() directly. Ideally it would use
  // StartMonitoringStartup() as the normal approach.
  friend class android::AfterStartupTaskUtilsJNI;

  static void SetBrowserStartupIsComplete();
};

#endif  // CHROME_BROWSER_AFTER_STARTUP_TASK_UTILS_H_
