// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/task_features.h"

#include "base/base_export.h"
#include "base/feature_list.h"

namespace base {

const Feature kMayBlockWithoutDelay = {"MayBlockWithoutDelay",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

const Feature kDisableJobYield = {"DisableJobYield",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

const Feature kDisableFairJobScheduling = {"DisableFairJobScheduling",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

const Feature kDisableJobUpdatePriority = {"DisableJobUpdatePriority",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

const Feature kWakeUpStrategyFeature = {"WakeUpStrategyFeature",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

constexpr FeatureParam<WakeUpStrategy>::Option kWakeUpStrategyOptions[] = {
    {WakeUpStrategy::kCentralizedWakeUps, "centralized-wakeups"},
    {WakeUpStrategy::kSerializedWakeUps, "serialized-wakeups"},
    {WakeUpStrategy::kExponentialWakeUps, "exponential-wakeups"},
    {WakeUpStrategy::kGreedyWakeUps, "greedy-wakeups"}};

const base::FeatureParam<WakeUpStrategy> kWakeUpStrategyParam{
    &kWakeUpStrategyFeature, "strategy", WakeUpStrategy::kExponentialWakeUps,
    &kWakeUpStrategyOptions};

const Feature kWakeUpAfterGetWork = {"WakeUpAfterGetWork",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

#if HAS_NATIVE_THREAD_POOL()
const Feature kUseNativeThreadPool = {"UseNativeThreadPool",
                                      base::FEATURE_DISABLED_BY_DEFAULT};
const Feature kUseBackgroundNativeThreadPool = {
    "UseBackgroundNativeThreadPool", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

const Feature kUseFiveMinutesThreadReclaimTime = {
    "UseFiveMinutesThreadReclaimTime", base::FEATURE_DISABLED_BY_DEFAULT};

// static
const BASE_EXPORT Feature kNoWakeUpsForCanceledTasks{
    "NoWakeUpsForCanceledTasks", FEATURE_DISABLED_BY_DEFAULT};

const BASE_EXPORT Feature kRemoveCanceledTasksInTaskQueue = {
    "RemoveCanceledTasksInTaskQueue2", base::FEATURE_DISABLED_BY_DEFAULT};

const BASE_EXPORT Feature kAlwaysAbandonScheduledTask = {
    "AlwaysAbandonScheduledTask", base::FEATURE_DISABLED_BY_DEFAULT};

const BASE_EXPORT Feature kAddTaskLeewayFeature = {
    "AddTaskLeeway", base::FEATURE_ENABLED_BY_DEFAULT};

const base::FeatureParam<TimeDelta> kTaskLeewayParam{&kAddTaskLeewayFeature,
                                                     "leeway", kDefaultLeeway};

const BASE_EXPORT Feature kAlignWakeUps = {"AlignWakeUps",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

const BASE_EXPORT Feature kExplicitHighResolutionTimerWin = {
    "ExplicitHighResolutionTimerWin", base::FEATURE_DISABLED_BY_DEFAULT};

const BASE_EXPORT Feature kRunTasksByBatches = {
    "RunTasksByBatches", base::FEATURE_DISABLED_BY_DEFAULT};

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
