// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AFTER_STARTUP_TASK_UTILS_H_
#define CHROME_BROWSER_AFTER_STARTUP_TASK_UTILS_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"

namespace android {
class AfterStartupTaskUtilsJNI;
}

namespace base {
class SequencedTaskRunner;
}

class AfterStartupTaskUtils {
 public:
#if !BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/528419929): Refactor AfterStartupTaskUtils.java on Android
  // to create a StartupInProgressRef instead of calling
  // SetBrowserStartupIsComplete() directly, enabling startup refs on Android.
  class StartupInProgressRef {
   public:
    explicit StartupInProgressRef(base::OnceClosure release_callback);
    ~StartupInProgressRef();
    StartupInProgressRef(const StartupInProgressRef&) = delete;
    StartupInProgressRef& operator=(const StartupInProgressRef&) = delete;

   private:
    base::ScopedClosureRunner release_runner_;
  };
#endif  // !BUILDFLAG(IS_ANDROID)

  AfterStartupTaskUtils() = delete;
  AfterStartupTaskUtils(const AfterStartupTaskUtils&) = delete;
  AfterStartupTaskUtils& operator=(const AfterStartupTaskUtils&) = delete;

#if !BUILDFLAG(IS_ANDROID)
  // Register a reference that delays startup completion. Startup is considered
  // complete only when all registered references are released.
  static std::unique_ptr<StartupInProgressRef> RegisterStartupInProgressRef();
#endif  // !BUILDFLAG(IS_ANDROID)

  // Signals the end of the startup registration phase, allowing startup to be
  // considered complete once all registered references are released.
  static void BeginMonitoringStartupCompletion();

  // For use by tests where BeginMonitoringStartupCompletion() may already have
  // been called or where we want to ensure monitoring has begun without
  // triggering duplicate initialization checks.
  static void BeginMonitoringStartupCompletionForTesting();

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
  // infrastructure and thus BeginMonitoringStartupCompletion() is unsuitable.
  static void SetBrowserStartupIsCompleteForTesting();

  static void UnsafeResetForTesting();

 private:
  // TODO(wkorman): Look into why Android calls SetBrowserStartupIsComplete()
  // directly. Ideally it would use BeginMonitoringStartupCompletion() as the
  // normal approach.
  friend class android::AfterStartupTaskUtilsJNI;

  static void SetBrowserStartupIsComplete();
};

#endif  // CHROME_BROWSER_AFTER_STARTUP_TASK_UTILS_H_
