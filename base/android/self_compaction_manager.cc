// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/self_compaction_manager.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/strcat.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/trace_event/trace_event.h"

namespace base::android {
BASE_FEATURE(kShouldFreezeSelf, "ShouldFreezeSelf", FEATURE_ENABLED_BY_DEFAULT);

// Max amount of compaction to do in each chunk, measured in MiB.
BASE_FEATURE_PARAM(size_t,
                   kShouldFreezeSelfMaxSize,
                   &kShouldFreezeSelf,
                   "max_chunk_size",
                   100);

// Delay between running pre-freeze tasks and doing self-freeze, measured in s.
BASE_FEATURE_PARAM(size_t,
                   kShouldFreezeSelfDelayAfterPreFreezeTasks,
                   &kShouldFreezeSelf,
                   "delay_after_tasks",
                   30);

BASE_FEATURE(kUseRunningCompact,
             "UseRunningCompact",
             FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(size_t,
                   kUseRunningCompactDelayAfterPreFreezeTasks,
                   &kUseRunningCompact,
                   "running_compact_delay_after_tasks",
                   30);
BASE_FEATURE_PARAM(size_t,
                   kUseRunningCompactMaxSize,
                   &kUseRunningCompact,
                   "running_compact_max_chunk_size",
                   10);

namespace {

uint64_t MiBToBytes(uint64_t v) {
  return v * 1024 * 1024;
}

std::string GetSelfCompactionMetricName(std::string_view name) {
  return StrCat({"Memory.SelfCompact2.Renderer.", name});
}

std::string GetRunningCompactionMetricName(std::string_view name) {
  return StrCat({"Memory.RunningCompact.Renderer.", name});
}

class SelfCompactionState final
    : public SelfCompactionManager::CompactionState {
 public:
  SelfCompactionState(scoped_refptr<SequencedTaskRunner> task_runner,
                      base::TimeTicks triggered_at)
      : SelfCompactionState(std::move(task_runner),
                            triggered_at,
                            MiBToBytes(kShouldFreezeSelfMaxSize.Get())) {}

  SelfCompactionState(scoped_refptr<SequencedTaskRunner> task_runner,
                      base::TimeTicks triggered_at,
                      uint64_t max_bytes)
      : CompactionState(std::move(task_runner), triggered_at, max_bytes) {}

  bool IsFeatureEnabled() const override {
    return base::FeatureList::IsEnabled(kShouldFreezeSelf);
  }

  base::TimeDelta GetDelayAfterPreFreezeTasks() const override {
    return base::Seconds(kShouldFreezeSelfDelayAfterPreFreezeTasks.Get());
  }

  std::string GetMetricName(std::string_view name) const override {
    return GetSelfCompactionMetricName(name);
  }

  scoped_refptr<SelfCompactionManager::CompactionMetric> MakeCompactionMetric(
      base::TimeTicks started_at) const override {
    return MakeRefCounted<SelfCompactionManager::CompactionMetric>(
        "Memory.SelfCompact2.Renderer.", triggered_at_, started_at);
  }
};

class RunningCompactionState final
    : public SelfCompactionManager::CompactionState {
 public:
  RunningCompactionState(scoped_refptr<SequencedTaskRunner> task_runner,
                         base::TimeTicks triggered_at)
      : RunningCompactionState(std::move(task_runner),
                               triggered_at,
                               MiBToBytes(kUseRunningCompactMaxSize.Get())) {}

  RunningCompactionState(scoped_refptr<SequencedTaskRunner> task_runner,
                         base::TimeTicks triggered_at,
                         uint64_t max_bytes)
      : CompactionState(std::move(task_runner), triggered_at, max_bytes) {}

  bool IsFeatureEnabled() const override {
    return base::FeatureList::IsEnabled(kUseRunningCompact);
  }

  base::TimeDelta GetDelayAfterPreFreezeTasks() const override {
    return base::Seconds(kUseRunningCompactDelayAfterPreFreezeTasks.Get());
  }

  std::string GetMetricName(std::string_view name) const override {
    return GetRunningCompactionMetricName(name);
  }

  scoped_refptr<SelfCompactionManager::CompactionMetric> MakeCompactionMetric(
      base::TimeTicks started_at) const override {
    return MakeRefCounted<SelfCompactionManager::CompactionMetric>(
        "Memory.RunningCompact.Renderer.", triggered_at_, started_at);
  }
};

}  // namespace

// static
void SelfCompactionManager::OnRunningCompact() {
  TRACE_EVENT0("base", "OnRunningCompact");

  auto task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::BEST_EFFORT, MayBlock()});
  Instance().OnTriggerCompact<RunningCompactionState>(std::move(task_runner));
}

// static
void SelfCompactionManager::OnSelfFreeze() {
  TRACE_EVENT0("base", "OnSelfFreeze");

  auto task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::BEST_EFFORT, MayBlock()});
  Instance().OnTriggerCompact<SelfCompactionState>(std::move(task_runner));
}

template <class State>
void SelfCompactionManager::OnTriggerCompact(
    scoped_refptr<SequencedTaskRunner> task_runner) {
  const auto triggered_at = base::TimeTicks::Now();
  base::AutoLock locker(lock());
  compaction_last_triggered_ = triggered_at;
  auto state = std::make_unique<State>(task_runner, triggered_at);
  OnTriggerCompact(std::move(state));
}

std::unique_ptr<SelfCompactionManager::CompactionState>
SelfCompactionManager::GetSelfCompactionStateForTesting(
    scoped_refptr<SequencedTaskRunner> task_runner,
    const TimeTicks& triggered_at) {
  return std::make_unique<SelfCompactionState>(std::move(task_runner),
                                               triggered_at, 1);
}

std::unique_ptr<SelfCompactionManager::CompactionState>
SelfCompactionManager::GetRunningCompactionStateForTesting(
    scoped_refptr<SequencedTaskRunner> task_runner,
    const TimeTicks& triggered_at) {
  return std::make_unique<RunningCompactionState>(std::move(task_runner),
                                                  triggered_at, 1);
}

}  // namespace base::android
