// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_TASK_FEATURES_H_
#define BASE_TASK_TASK_FEATURES_H_

#include "base/base_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace base {

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE)
#define HAS_NATIVE_THREAD_POOL() 1
#else
#define HAS_NATIVE_THREAD_POOL() 0
#endif

#if HAS_NATIVE_THREAD_POOL()
// Under this feature, ThreadPoolImpl will use a foreground ThreadGroup backed
// by a native thread pool implementation. The Windows Thread Pool API and
// libdispatch are used on Windows and macOS/iOS respectively.
BASE_EXPORT BASE_DECLARE_FEATURE(kUseNativeThreadPool);
// Under this feature, ThreadPoolImpl will use a background ThreadGroup backed
// by a native thread pool implementation.
BASE_EXPORT BASE_DECLARE_FEATURE(kUseBackgroundNativeThreadPool);
#endif

// Under this feature, worker threads are not reclaimed after a timeout. Rather,
// only excess workers are cleaned up immediately after finishing a task.
BASE_EXPORT BASE_DECLARE_FEATURE(kNoWorkerThreadReclaim);

// This feature controls whether wake ups are possible for canceled tasks.
BASE_EXPORT BASE_DECLARE_FEATURE(kNoWakeUpsForCanceledTasks);

// Controls whether or not canceled delayed tasks are removed from task queues.
BASE_EXPORT BASE_DECLARE_FEATURE(kRemoveCanceledTasksInTaskQueue);

// This feature controls whether or not the scheduled task is always abandoned
// when a timer is stopped or reset. The re-use of the scheduled task is an
// optimization that ensures a timer can not leave multiple canceled tasks in
// the task queue. Meant to be used in conjunction with
// kRemoveCanceledTasksInTaskQueue.
BASE_EXPORT BASE_DECLARE_FEATURE(kAlwaysAbandonScheduledTask);

// Under this feature, a non-zero leeway is added to delayed tasks. Along with
// DelayPolicy, this affects the time at which a delayed task runs.
BASE_EXPORT BASE_DECLARE_FEATURE(kAddTaskLeewayFeature);
constexpr TimeDelta kDefaultLeeway = Milliseconds(8);
extern const BASE_EXPORT base::FeatureParam<TimeDelta> kTaskLeewayParam;

// Under this feature, wake ups are aligned at a 8ms boundary when allowed per
// DelayPolicy.
BASE_EXPORT BASE_DECLARE_FEATURE(kAlignWakeUps);

// Under this feature, tasks that need high resolution timer are determined
// based on explicit DelayPolicy rather than based on a threshold.
BASE_EXPORT BASE_DECLARE_FEATURE(kExplicitHighResolutionTimerWin);

// Feature to run tasks by batches before pumping out messages.
BASE_EXPORT BASE_DECLARE_FEATURE(kRunTasksByBatches);

// Feature to run tasks by batches before pumping out messages.
BASE_EXPORT BASE_DECLARE_FEATURE(kBrowserPeriodicYieldingToNative);
extern const BASE_EXPORT base::FeatureParam<TimeDelta>
    kBrowserPeriodicYieldingToNativeNormalInputAfterMsParam;
extern const BASE_EXPORT base::FeatureParam<TimeDelta>
    kBrowserPeriodicYieldingToNativeFlingInputAfterMsParam;
extern const BASE_EXPORT base::FeatureParam<TimeDelta>
    kBrowserPeriodicYieldingToNativeNoInputAfterMsParam;
extern const BASE_EXPORT base::FeatureParam<TimeDelta>
    kBrowserPeriodicYieldingToNativeDelay;

BASE_EXPORT void InitializeTaskLeeway();
BASE_EXPORT TimeDelta GetTaskLeeway();

}  // namespace base

#endif  // BASE_TASK_TASK_FEATURES_H_
