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

// Fixed amount of threads that will be used as a cap for thread pools.
BASE_EXPORT BASE_DECLARE_FEATURE(kThreadPoolCap2);

extern const BASE_EXPORT base::FeatureParam<int> kThreadPoolCapRestrictedCount;

// Under this feature, a utility_thread_group will be created for
// running USER_VISIBLE tasks.
BASE_EXPORT BASE_DECLARE_FEATURE(kUseUtilityThreadGroup);

// Under this feature, worker threads are not reclaimed after a timeout. Rather,
// only excess workers are cleaned up immediately after finishing a task.
BASE_EXPORT BASE_DECLARE_FEATURE(kNoWorkerThreadReclaim);

// This feature controls whether wake ups are possible for canceled tasks.
BASE_EXPORT BASE_DECLARE_FEATURE(kNoWakeUpsForCanceledTasks);

// Controls whether or not canceled delayed tasks are removed from task queues.
BASE_EXPORT BASE_DECLARE_FEATURE(kRemoveCanceledTasksInTaskQueue);

// This feature controls whether ThreadPool WorkerThreads should hold off waking
// up to purge partition alloc within the first minute of their lifetime. See
// base::internal::GetSleepTimeBeforePurge.
BASE_EXPORT BASE_DECLARE_FEATURE(kDelayFirstWorkerWake);

// Under this feature, a non-zero leeway is added to delayed tasks. Along with
// DelayPolicy, this affects the time at which a delayed task runs.
BASE_EXPORT BASE_DECLARE_FEATURE(kAddTaskLeewayFeature);
#if BUILDFLAG(IS_WIN)
constexpr TimeDelta kDefaultLeeway = Milliseconds(16);
#else
constexpr TimeDelta kDefaultLeeway = Milliseconds(8);
#endif  // #if !BUILDFLAG(IS_WIN)
extern const BASE_EXPORT base::FeatureParam<TimeDelta> kTaskLeewayParam;

// Under this feature, wake ups are aligned at a 8ms boundary when allowed per
// DelayPolicy.
BASE_EXPORT BASE_DECLARE_FEATURE(kAlignWakeUps);

// Under this feature, tasks that need high resolution timer are determined
// based on explicit DelayPolicy rather than based on a threshold.
BASE_EXPORT BASE_DECLARE_FEATURE(kExplicitHighResolutionTimerWin);

// Feature to run tasks by batches before pumping out messages.
BASE_EXPORT BASE_DECLARE_FEATURE(kRunTasksByBatches);

BASE_EXPORT void InitializeTaskLeeway();
BASE_EXPORT TimeDelta GetTaskLeewayForCurrentThread();
BASE_EXPORT TimeDelta GetDefaultTaskLeeway();

// Controls the max number of delayed tasks that can run before selecting an
// immediate task in sequence manager.
BASE_EXPORT BASE_DECLARE_FEATURE(kMaxDelayedStarvationTasks);
extern const BASE_EXPORT base::FeatureParam<int>
    kMaxDelayedStarvationTasksParam;

}  // namespace base

#endif  // BASE_TASK_TASK_FEATURES_H_
