// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/work_deduplicator.h"

#include <ostream>
#include <utility>
#include "base/check_op.h"

namespace base {
namespace sequence_manager {
namespace internal {

WorkDeduplicator::WorkDeduplicator(
    scoped_refptr<const AssociatedThreadId> associated_thread)
    : associated_thread_(std::move(associated_thread)) {}

WorkDeduplicator::~WorkDeduplicator() = default;

WorkDeduplicator::ShouldScheduleWork WorkDeduplicator::BindToCurrentThread() {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  int previous_flags = state_.fetch_or(kBoundFlag);
  DCHECK_EQ(previous_flags & kBoundFlag, 0) << "Can't bind twice!";
  return previous_flags & kPendingDoWorkFlag
             ? ShouldScheduleWork::kScheduleImmediate
             : ShouldScheduleWork::kNotNeeded;
}

WorkDeduplicator::ShouldScheduleWork WorkDeduplicator::OnWorkRequested() {
  // Set kPendingDoWorkFlag and return true if we were previously kIdle.
  return state_.fetch_or(kPendingDoWorkFlag) == State::kIdle
             ? ShouldScheduleWork::kScheduleImmediate
             : ShouldScheduleWork::kNotNeeded;
}

WorkDeduplicator::ShouldScheduleWork WorkDeduplicator::OnDelayedWorkRequested()
    const {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  // This must be called on the associated thread or this read is racy.
  return state_.load() == State::kIdle ? ShouldScheduleWork::kScheduleImmediate
                                       : ShouldScheduleWork::kNotNeeded;
}

void WorkDeduplicator::OnWorkStarted() {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  DCHECK_EQ(state_.load() & kBoundFlag, kBoundFlag);
  // Clear kPendingDoWorkFlag and mark us as in a DoWork.
  state_.store(State::kInDoWork);
}

void WorkDeduplicator::WillCheckForMoreWork() {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  DCHECK_EQ(state_.load() & kBoundFlag, kBoundFlag);
  // Clear kPendingDoWorkFlag if it was set.
  state_.store(State::kInDoWork);
}

WorkDeduplicator::ShouldScheduleWork WorkDeduplicator::DidCheckForMoreWork(
    NextTask next_task) {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  DCHECK_EQ(state_.load() & kBoundFlag, kBoundFlag);
  if (next_task == NextTask::kIsImmediate) {
    state_.store(State::kDoWorkPending);
    return ShouldScheduleWork::kScheduleImmediate;
  }
  // If |next_task| is not immediate, there's still a possibility that
  // OnWorkRequested() was invoked racily from another thread just after this
  // thread determined that the next task wasn't immediate. In that case, that
  // other thread relies on us to return kScheduleImmediate.
  return (state_.fetch_and(~kInDoWorkFlag) & kPendingDoWorkFlag)
             ? ShouldScheduleWork::kScheduleImmediate
             : ShouldScheduleWork::kNotNeeded;
}

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base
