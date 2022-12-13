// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/task_features.h"

#include <atomic>

#include "base/base_export.h"
#include "base/feature_list.h"
#include "base/threading/platform_thread.h"

namespace base {

// Note to implementers: thread pool code using task features must absolutely
// not invoke FeatureList::IsEnabled outside of the main thread. Doing so
// causes data races between worker threads and ~FeatureList when tests end
// (crbug.com/1344573). A reliable moment to query and cache the feature state
// is on ThreadPoolImpl::Start (and thus also on the first WorkerThread::Start,
// not the later ones) as this is invoked from the main thread after
// initializing the FeatureList. If caching the feature state in a static, you
// must be aware that all tests sharing a process will have the same state,
// regardless of future ScopedFeatureList instances.

BASE_FEATURE(kUseUtilityThreadGroup,
             "UseUtilityThreadGroup",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNoWorkerThreadReclaim,
             "NoWorkerThreadReclaim",
             base::FEATURE_DISABLED_BY_DEFAULT);

// static
BASE_FEATURE(kNoWakeUpsForCanceledTasks,
             "NoWakeUpsForCanceledTasks",
             FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kRemoveCanceledTasksInTaskQueue,
             "RemoveCanceledTasksInTaskQueue2",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAlwaysAbandonScheduledTask,
             "AlwaysAbandonScheduledTask",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDelayFirstWorkerWake,
             "DelayFirstWorkerWake",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAddTaskLeewayFeature,
             "AddTaskLeeway",
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<TimeDelta> kTaskLeewayParam{&kAddTaskLeewayFeature,
                                                     "leeway", kDefaultLeeway};

BASE_FEATURE(kAlignWakeUps, "AlignWakeUps", base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kExplicitHighResolutionTimerWin,
             "ExplicitHighResolutionTimerWin",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kRunTasksByBatches,
             "RunTasksByBatches",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kBrowserPeriodicYieldingToNative,
             "BrowserPeriodicYieldingToNative",
             base::FEATURE_DISABLED_BY_DEFAULT);

const BASE_EXPORT base::FeatureParam<base::TimeDelta>
    kBrowserPeriodicYieldingToNativeNormalInputAfterMsParam{
        &kBrowserPeriodicYieldingToNative,
        "yield_to_android_looper_after_ms_normal_input", base::Milliseconds(8)};

const BASE_EXPORT base::FeatureParam<base::TimeDelta>
    kBrowserPeriodicYieldingToNativeFlingInputAfterMsParam{
        &kBrowserPeriodicYieldingToNative,
        "yield_to_android_looper_after_ms_fling_input", base::Milliseconds(16)};

const BASE_EXPORT base::FeatureParam<base::TimeDelta>
    kBrowserPeriodicYieldingToNativeNoInputAfterMsParam{
        &kBrowserPeriodicYieldingToNative,
        "yield_to_android_looper_after_ms_no_input", base::Milliseconds(100)};

const BASE_EXPORT base::FeatureParam<base::TimeDelta>
    kBrowserPeriodicYieldingToNativeDelay{&kBrowserPeriodicYieldingToNative,
                                          "non_delayed_looper_defer_for_ns",
                                          base::Nanoseconds(500000)};

// Leeway value applied to delayed tasks. An atomic is used here because the
// value is queried from multiple threads.
std::atomic<TimeDelta> g_task_leeway{kDefaultLeeway};

BASE_EXPORT void InitializeTaskLeeway() {
  g_task_leeway.store(kTaskLeewayParam.Get(), std::memory_order_relaxed);
}

BASE_EXPORT TimeDelta GetTaskLeewayForCurrentThread() {
  // For some threads, there might be a override of the leeway, so check it
  // first.
  auto leeway_override = PlatformThread::GetThreadLeewayOverride();
  if (leeway_override.has_value())
    return leeway_override.value();
  return g_task_leeway.load(std::memory_order_relaxed);
}

BASE_EXPORT TimeDelta GetDefaultTaskLeeway() {
  return g_task_leeway.load(std::memory_order_relaxed);
}

}  // namespace base
