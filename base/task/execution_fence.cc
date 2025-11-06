// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/execution_fence.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/features.h"
#include "base/no_destructor.h"
#include "base/not_fatal_until.h"
#include "base/sequence_checker.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace base {

using sequence_manager::SequenceManager;
using sequence_manager::TaskQueue;

namespace {

// Global list of SequenceManagers to notify of ScopedBestEffortExecutionFences.
class SequenceManagerRegistry {
 public:
  SequenceManagerRegistry() = default;
  ~SequenceManagerRegistry() = default;

  SequenceManagerRegistry(const SequenceManagerRegistry&) = delete;
  SequenceManagerRegistry& operator=(const SequenceManagerRegistry&) = delete;

  static SequenceManagerRegistry& GetInstance() {
    static NoDestructor<SequenceManagerRegistry> instance;
    return *instance;
  }

  void AddSequenceManager(SequenceManager* sequence_manager) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    auto [_, inserted] = sequence_managers_.emplace(sequence_manager);
    CHECK(inserted, NotFatalUntil::M145);
  }

  void RemoveSequenceManager(SequenceManager* sequence_manager) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    size_t erased = sequence_managers_.erase(sequence_manager);
    CHECK_EQ(erased, 1u, NotFatalUntil::M145);
  }

  std::vector<std::unique_ptr<TaskQueue::QueueEnabledVoter>>
  GetCurrentVoters() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    std::vector<std::unique_ptr<TaskQueue::QueueEnabledVoter>> voters;
    for (auto* sequence_manager : sequence_managers_) {
      for (TaskQueue* task_queue :
           sequence_manager->GetBestEffortTaskQueues()) {
        if (task_queue->IsBlockedByScopedExecutionFences()) {
          voters.push_back(task_queue->CreateQueueEnabledVoter());
          voters.back()->SetVoteToEnable(false);
        }
      }
    }
    return voters;
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  absl::flat_hash_set<SequenceManager*> sequence_managers_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace

ScopedThreadPoolExecutionFence::ScopedThreadPoolExecutionFence() {
  auto* thread_pool = ThreadPoolInstance::Get();
  CHECK(thread_pool, NotFatalUntil::M145);
  thread_pool->BeginFence();
}

ScopedThreadPoolExecutionFence::~ScopedThreadPoolExecutionFence() {
  auto* thread_pool = ThreadPoolInstance::Get();
  CHECK(thread_pool, NotFatalUntil::M145);
  thread_pool->EndFence();
}

ScopedBestEffortExecutionFence::ScopedBestEffortExecutionFence() {
  if (FeatureList::IsEnabled(
          features::kScopedBestEffortExecutionFenceForTaskQueue)) {
    task_queue_voters_ =
        SequenceManagerRegistry::GetInstance().GetCurrentVoters();
  }
  auto* thread_pool = ThreadPoolInstance::Get();
  CHECK(thread_pool, NotFatalUntil::M145);
  thread_pool->BeginBestEffortFence();
}

ScopedBestEffortExecutionFence::~ScopedBestEffortExecutionFence() {
  auto* thread_pool = ThreadPoolInstance::Get();
  CHECK(thread_pool, NotFatalUntil::M145);
  thread_pool->EndBestEffortFence();
}

// static
void ScopedBestEffortExecutionFence::AddSequenceManager(
    SequenceManager* sequence_manager) {
  SequenceManagerRegistry::GetInstance().AddSequenceManager(sequence_manager);
}

// static
void ScopedBestEffortExecutionFence::RemoveSequenceManager(
    SequenceManager* sequence_manager) {
  SequenceManagerRegistry::GetInstance().RemoveSequenceManager(
      sequence_manager);
}

}  // namespace base
