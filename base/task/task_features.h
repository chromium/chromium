// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_TASK_FEATURES_H_
#define BASE_TASK_TASK_FEATURES_H_

#include "base/base_export.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace base {

struct Feature;

// Under this feature, workers blocked with MayBlock are replaced immediately
// instead of waiting for a threshold in the foreground thread group.
extern const BASE_EXPORT Feature kMayBlockWithoutDelay;

// Under this feature, ThreadPool::ShouldYield() always returns false
extern const BASE_EXPORT Feature kDisableJobYield;
// Under this feature, JobTaskSource doesn't use worker count in its sort key
// such that worker threads are not distributed among running jobs equally.
extern const BASE_EXPORT Feature kDisableFairJobScheduling;
// Under this feature, priority update on Jobs is disabled.
extern const BASE_EXPORT Feature kDisableJobUpdatePriority;
// Under this feature, another WorkerThread is signaled only after the current
// thread was assigned work.
extern const BASE_EXPORT Feature kWakeUpAfterGetWork;

// Strategy affecting how WorkerThreads are signaled to pick up pending work.
enum class WakeUpStrategy {
  // A single thread scheduling new work signals all required WorkerThreads.
  kCentralizedWakeUps,
  // Each thread signals at most a single thread, either when scheduling new
  // work or picking up pending work.
  kSerializedWakeUps,
  // Each thread signals at most 2 threads, either when scheduling new
  // work or picking up pending work.
  kExponentialWakeUps,
  // Each thread signals as many threads as necessary, either when scheduling
  // new work or picking up pending work.
  kGreedyWakeUps,
};

// Under this feature, a given WakeUpStrategy param is used.
extern const BASE_EXPORT Feature kWakeUpStrategyFeature;
extern const BASE_EXPORT base::FeatureParam<WakeUpStrategy>
    kWakeUpStrategyParam;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE)
#define HAS_NATIVE_THREAD_POOL() 1
#else
#define HAS_NATIVE_THREAD_POOL() 0
#endif

#if HAS_NATIVE_THREAD_POOL()
// Under this feature, ThreadPoolImpl will use a foreground ThreadGroup backed
// by a native thread pool implementation. The Windows Thread Pool API and
// libdispatch are used on Windows and macOS/iOS respectively.
extern const BASE_EXPORT Feature kUseNativeThreadPool;
// Under this feature, ThreadPoolImpl will use a background ThreadGroup backed
// by a native thread pool implementation.
extern const BASE_EXPORT Feature kUseBackgroundNativeThreadPool;
#endif

// Whether threads in the ThreadPool should be reclaimed after being idle for 5
// minutes, instead of 30 seconds.
extern const BASE_EXPORT Feature kUseFiveMinutesThreadReclaimTime;

// This feature controls whether wake ups are possible for canceled tasks.
extern const BASE_EXPORT Feature kNoWakeUpsForCanceledTasks;

// Controls whether or not canceled delayed tasks are removed from task queues.
extern const BASE_EXPORT base::Feature kRemoveCanceledTasksInTaskQueue;

// This feature controls whether or not the scheduled task is always abandoned
// when a timer is stopped or reset. The re-use of the scheduled task is an
// optimization that ensures a timer can not leave multiple canceled tasks in
// the task queue. Meant to be used in conjunction with
// kRemoveCanceledTasksInTaskQueue.
extern const BASE_EXPORT base::Feature kAlwaysAbandonScheduledTask;

// Under this feature, a non-zero leeway is added to delayed tasks. Along with
// DelayPolicy, this affects the time at which a delayed task runs.
extern const BASE_EXPORT Feature kAddTaskLeewayFeature;
constexpr TimeDelta kDefaultLeeway = Milliseconds(8);
extern const BASE_EXPORT base::FeatureParam<TimeDelta> kTaskLeewayParam;

// Under this feature, wake ups are aligned at a 8ms boundary when allowed per
// DelayPolicy.
extern const BASE_EXPORT base::Feature kAlignWakeUps;

// Under this feature, tasks that need high resolution timer are determined
// based on explicit DelayPolicy rather than based on a threshold.
extern const BASE_EXPORT base::Feature kExplicitHighResolutionTimerWin;

// Feature to run tasks by batches before pumping out messages.
extern const BASE_EXPORT base::Feature kRunTasksByBatches;

BASE_EXPORT void InitializeTaskLeeway();
BASE_EXPORT TimeDelta GetTaskLeeway();

}  // namespace base

#endif  // BASE_TASK_TASK_FEATURES_H_
