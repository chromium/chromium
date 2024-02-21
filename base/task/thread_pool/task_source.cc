// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/task_source.h"

#include <utility>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/task/task_features.h"
#include "base/task/thread_pool/task_tracker.h"

namespace base {
namespace internal {

ExecutionEnvironment::~ExecutionEnvironment() = default;

TaskSource::Transaction::Transaction(TaskSource* task_source)
    : task_source_(task_source) {
  task_source->lock_.Acquire();
}

TaskSource::Transaction::Transaction(TaskSource::Transaction&& other)
    : task_source_(other.task_source()) {
  other.task_source_ = nullptr;
}

TaskSource::Transaction::~Transaction() {
  if (task_source_) {
    Release();
  }
}

void TaskSource::Transaction::UpdatePriority(TaskPriority priority) {
  task_source_->traits_.UpdatePriority(priority);
  task_source_->priority_racy_.store(task_source_->traits_.priority(),
                                     std::memory_order_relaxed);
}

void TaskSource::Transaction::Release() NO_THREAD_SAFETY_ANALYSIS {
  DCHECK(task_source_);
  task_source_->lock_.AssertAcquired();
  task_source_->lock_.Release();
  task_source_ = nullptr;
}

void TaskSource::SetImmediateHeapHandle(const HeapHandle& handle) {
  immediate_pq_heap_handle_ = handle;
}

void TaskSource::ClearImmediateHeapHandle() {
  immediate_pq_heap_handle_ = HeapHandle();
}

void TaskSource::SetDelayedHeapHandle(const HeapHandle& handle) {
  delayed_pq_heap_handle_ = handle;
}

void TaskSource::ClearDelayedHeapHandle() {
  delayed_pq_heap_handle_ = HeapHandle();
}

TaskSource::TaskSource(const TaskTraits& traits,
                       TaskSourceExecutionMode execution_mode)
    : traits_(traits),
      priority_racy_(traits.priority()),
      execution_mode_(execution_mode) {}

TaskSource::~TaskSource() {
  // If this fails, a Transaction was likely held while releasing a reference to
  // its associated task source, which lead to its destruction. Owners of
  // Transaction must ensure to hold onto a reference of the associated task
  // source at least until the Transaction is released to prevent UAF.
  lock_.AssertNotHeld();
}

TaskSource::Transaction TaskSource::BeginTransaction() {
  return Transaction(this);
}

void TaskSource::ClearForTesting() {
  auto task = Clear(nullptr);
  if (task) {
    std::move(task->task).Run();
  }
}

RegisteredTaskSource::RegisteredTaskSource() = default;

RegisteredTaskSource::RegisteredTaskSource(std::nullptr_t)
    : RegisteredTaskSource() {}

RegisteredTaskSource::RegisteredTaskSource(
    RegisteredTaskSource&& other) noexcept
    :
#if DCHECK_IS_ON()
      run_step_{std::exchange(other.run_step_, State::kInitial)},
#endif  // DCHECK_IS_ON()
      task_source_{std::move(other.task_source_)},
      task_tracker_{std::exchange(other.task_tracker_, nullptr)} {
}

RegisteredTaskSource::~RegisteredTaskSource() {
  Unregister();
}

//  static
RegisteredTaskSource RegisteredTaskSource::CreateForTesting(
    scoped_refptr<TaskSource> task_source,
    TaskTracker* task_tracker) {
  return RegisteredTaskSource(std::move(task_source), task_tracker);
}

scoped_refptr<TaskSource> RegisteredTaskSource::Unregister() {
#if DCHECK_IS_ON()
  DCHECK_EQ(run_step_, State::kInitial);
#endif  // DCHECK_IS_ON()
  if (task_source_ && task_tracker_)
    return task_tracker_->UnregisterTaskSource(std::move(task_source_));
  return std::move(task_source_);
}

RegisteredTaskSource& RegisteredTaskSource::operator=(
    RegisteredTaskSource&& other) {
  Unregister();
#if DCHECK_IS_ON()
  run_step_ = std::exchange(other.run_step_, State::kInitial);
#endif  // DCHECK_IS_ON()
  task_source_ = std::move(other.task_source_);
  task_tracker_ = std::exchange(other.task_tracker_, nullptr);
  return *this;
}

TaskSource::RunStatus RegisteredTaskSource::WillRunTask() {
  TaskSource::RunStatus run_status = task_source_->WillRunTask();
#if DCHECK_IS_ON()
  DCHECK_EQ(run_step_, State::kInitial);
  if (run_status != TaskSource::RunStatus::kDisallowed)
    run_step_ = State::kReady;
#endif  // DCHECK_IS_ON()
  return run_status;
}

Task RegisteredTaskSource::TakeTask(TaskSource::Transaction* transaction) {
  DCHECK(!transaction || transaction->task_source() == get());
#if DCHECK_IS_ON()
  DCHECK_EQ(State::kReady, run_step_);
#endif  // DCHECK_IS_ON()
  return task_source_->TakeTask(transaction);
}

std::optional<Task> RegisteredTaskSource::Clear(
    TaskSource::Transaction* transaction) {
  DCHECK(!transaction || transaction->task_source() == get());
  return task_source_->Clear(transaction);
}

bool RegisteredTaskSource::DidProcessTask(
    TaskSource::Transaction* transaction) {
  DCHECK(!transaction || transaction->task_source() == get());
#if DCHECK_IS_ON()
  DCHECK_EQ(State::kReady, run_step_);
  run_step_ = State::kInitial;
#endif  // DCHECK_IS_ON()
  return task_source_->DidProcessTask(transaction);
}

bool RegisteredTaskSource::WillReEnqueue(TimeTicks now,
                                         TaskSource::Transaction* transaction) {
  DCHECK(!transaction || transaction->task_source() == get());
#if DCHECK_IS_ON()
  DCHECK_EQ(State::kInitial, run_step_);
#endif  // DCHECK_IS_ON()
  return task_source_->WillReEnqueue(now, transaction);
}

RegisteredTaskSource::RegisteredTaskSource(
    scoped_refptr<TaskSource> task_source,
    TaskTracker* task_tracker)
    : task_source_(std::move(task_source)), task_tracker_(task_tracker) {}

RegisteredTaskSourceAndTransaction::RegisteredTaskSourceAndTransaction(
    RegisteredTaskSource task_source_in,
    TaskSource::Transaction transaction_in)
    : task_source(std::move(task_source_in)),
      transaction(std::move(transaction_in)) {
  DCHECK_EQ(task_source.get(), transaction.task_source());
}

// static:
RegisteredTaskSourceAndTransaction
RegisteredTaskSourceAndTransaction::FromTaskSource(
    RegisteredTaskSource task_source_in) {
  auto transaction = task_source_in->BeginTransaction();
  return RegisteredTaskSourceAndTransaction(std::move(task_source_in),
                                            std::move(transaction));
}

TaskSourceAndTransaction::TaskSourceAndTransaction(
    TaskSourceAndTransaction&& other) = default;

TaskSourceAndTransaction::~TaskSourceAndTransaction() = default;

TaskSourceAndTransaction::TaskSourceAndTransaction(
    scoped_refptr<TaskSource> task_source_in,
    TaskSource::Transaction transaction_in)
    : task_source(std::move(task_source_in)),
      transaction(std::move(transaction_in)) {
  DCHECK_EQ(task_source.get(), transaction.task_source());
}

// static:
TaskSourceAndTransaction TaskSourceAndTransaction::FromTaskSource(
    scoped_refptr<TaskSource> task_source_in) {
  auto transaction = task_source_in->BeginTransaction();
  return TaskSourceAndTransaction(std::move(task_source_in),
                                  std::move(transaction));
}

}  // namespace internal
}  // namespace base
