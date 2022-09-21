// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/task_features.h"

#include "base/base_export.h"
#include "base/feature_list.h"

namespace base {

#if HAS_NATIVE_THREAD_POOL()
BASE_FEATURE(kUseNativeThreadPool,
             "UseNativeThreadPool",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kUseBackgroundNativeThreadPool,
             "UseBackgroundNativeThreadPool",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kNoWorkerThreadReclaim,
             "NoWorkerThreadReclaim",
             base::FEATURE_DISABLED_BY_DEFAULT);

// static
BASE_FEATURE(kNoWakeUpsForCanceledTasks,
             "NoWakeUpsForCanceledTasks",
             FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kRemoveCanceledTasksInTaskQueue,
             "RemoveCanceledTasksInTaskQueue2",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAlwaysAbandonScheduledTask,
             "AlwaysAbandonScheduledTask",
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

BASE_EXPORT TimeDelta GetTaskLeeway() {
  return g_task_leeway.load(std::memory_order_relaxed);
}

}  // namespace base
