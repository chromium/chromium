// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AFTER_STARTUP_TASK_UTILS_H_
#define CHROME_BROWSER_AFTER_STARTUP_TASK_UTILS_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace android {
class AfterStartupTaskUtilsJNI;
}

namespace base {
class SequencedTaskRunner;
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(StartupIsCompleteReason)
enum class StartupIsCompleteReason {
  kFailsafeTimeout = 0,
  kStartupRegistrationDone = 1,
  kFirstWebContentsProfiler = 2,
  kSessionRestore = 3,
  kFirstIdle = 4,
  kChromeOSLoginScreen = 5,
  kAndroidStartup = 6,
  kVisiblePageLoadingFinished = 7,
  kVisiblePageLoadingTimedOut = 8,
  kMaxValue = kVisiblePageLoadingTimedOut,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/startup/enums.xml:StartupIsCompleteReason)

class AfterStartupTaskUtils {
 public:
#if !BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/528419929): Refactor AfterStartupTaskUtils.java on Android
  // to create a StartupInProgressRef instead of calling
  // SetBrowserStartupIsComplete() directly, enabling startup refs on Android.
  class StartupInProgressRef {
   public:
    explicit StartupInProgressRef(StartupIsCompleteReason reason);
    ~StartupInProgressRef();

    StartupInProgressRef(const StartupInProgressRef&) = delete;
    StartupInProgressRef& operator=(const StartupInProgressRef&) = delete;

    // Changes the reason that will be logged when this ref is dropped. For
    // example if a ref could be dropped when an operation completes, or on
    // timeout, this could be set to the appropriate reason before deleting it.
    void SetStartupIsCompleteReason(StartupIsCompleteReason reason) {
      reason_ = reason;
    }

   private:
    StartupIsCompleteReason reason_;
  };
#endif  // !BUILDFLAG(IS_ANDROID)

  AfterStartupTaskUtils() = delete;
  AfterStartupTaskUtils(const AfterStartupTaskUtils&) = delete;
  AfterStartupTaskUtils& operator=(const AfterStartupTaskUtils&) = delete;

#if !BUILDFLAG(IS_ANDROID)
  // Register a reference that delays startup completion. Startup is considered
  // complete only when all registered references are released.
  static std::unique_ptr<StartupInProgressRef> RegisterStartupInProgressRef(
      StartupIsCompleteReason reason);
#endif  // !BUILDFLAG(IS_ANDROID)

  // Signals the end of the startup registration phase, allowing startup to be
  // considered complete once all registered references are released.
  static void BeginMonitoringStartupCompletion();

  // For use by tests where BeginMonitoringStartupCompletion() may already have
  // been called or where we want to ensure monitoring has begun without
  // triggering duplicate initialization checks. If `include_default_refs` is
  // true a set of standard StartupInProgressRefs will be registered first. The
  // standard refs are always registered by the production
  // BeginMonitoringStartupCompletion().
  static void BeginMonitoringStartupCompletionForTesting(
      bool include_default_refs);

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
  static void SetBrowserStartupIsCompleteForTesting(
      StartupIsCompleteReason reason =
          StartupIsCompleteReason::kFailsafeTimeout);

  static void UnsafeResetForTesting();

  // Returns the timeout until startup is declared "finished" even if not all
  // StartupInProgressRefs have been dropped.
  static base::TimeDelta GetFailsafeTimeout();

 private:
  // TODO(wkorman): Look into why Android calls SetBrowserStartupIsComplete()
  // directly. Ideally it would use BeginMonitoringStartupCompletion() as the
  // normal approach.
  friend class android::AfterStartupTaskUtilsJNI;

  static void FinishStartupRegistration(bool include_default_refs);
  static void SetBrowserStartupIsComplete(StartupIsCompleteReason reason);
};

#endif  // CHROME_BROWSER_AFTER_STARTUP_TASK_UTILS_H_
