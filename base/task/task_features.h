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

#if defined(OS_WIN) || defined(OS_APPLE)
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

}  // namespace base

#endif  // BASE_TASK_TASK_FEATURES_H_
