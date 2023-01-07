// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_THREAD_POOL_SEQUENCE_H_
#define BASE_TASK_THREAD_POOL_SEQUENCE_H_

#include <stddef.h>

#include "base/base_export.h"
#include "base/containers/intrusive_heap.h"
#include "base/containers/queue.h"
#include "base/sequence_token.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool/pooled_parallel_task_runner.h"
#include "base/task/thread_pool/task.h"
#include "base/task/thread_pool/task_source.h"
#include "base/task/thread_pool/task_source_sort_key.h"
#include "base/threading/sequence_local_storage_map.h"

namespace base {
namespace internal {

// A Sequence is intended to hold delayed tasks and immediate tasks.
// Delayed tasks are held in a prority_queue until they are ripe and
// immediate tasks in a simple fifo queue.
// Sequence::PushTask is responsible for putting a task into the right
// queue depending on its nature.
// When Sequence::TakeTask is called, we select the next appropriate task
// from both queues and return it.
// Each queue holds slots each containing up to a single Task that must be
// executed in posting/runtime order.
//
// In comments below, an "empty Sequence" is a Sequence with no slot.
//
// Note: there is a known refcounted-ownership cycle in the Scheduler
// architecture: Sequence -> Task -> TaskRunner -> Sequence -> ...
// This is okay so long as the other owners of Sequence (PriorityQueue and
// WorkerThread in alternation and
// ThreadGroupImpl::WorkerThreadDelegateImpl::GetWork()
// temporarily) keep running it (and taking Tasks from it as a result). A
// dangling reference cycle would only occur should they release their reference
// to it while it's not empty. In other words, it is only correct for them to
// release it after PopTask() returns false to indicate it was made empty by
// that call (in which case the next PushImmediateTask() will return true to
// indicate to the caller that the Sequence should be re-enqueued for
// execution). This class is thread-safe.
class BASE_EXPORT Sequence : public TaskSource {
 public:
  // A Transaction can perform multiple operations atomically on a
  // Sequence. While a Transaction is alive, it is guaranteed that nothing
  // else will access the Sequence; the Sequence's lock is held for the
  // lifetime of the Transaction.
  class BASE_EXPORT Transaction : public TaskSource::Transaction {
   public:
    Transaction(Transaction&& other);
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;
    ~Transaction();

    // Returns true if the sequence would need to be queued in the
    // immediate/delayed queue after receiving a new immediate/delayed Task.
    // Thread-safe but the returned value may immediately be obsolete when
    // pushing a delayed task since a sequence can become ready at any time;
    // therefore it must be externally synchronized to prevent races against
    // OnBecomeReady().
    [[nodiscard]] bool ShouldBeQueued() const;

    // Returns true if the task to be posted will change the sequence
    // delayed_queue top.
    bool TopDelayedTaskWillChange(Task& delayed_task) const;

    // Adds immediate |task| to the end of this sequence. This must only
    // be called after invoking ShouldBeQueued().
    void PushImmediateTask(Task task);

    // Adds a delayed |task| in this sequence to be prioritized based on it's
    // delayed run time. This must only be called after invoking
    // TopDelayedTaskWillChange()/ShouldBeQueued().
    void PushDelayedTask(Task task);

    Sequence* sequence() const { return static_cast<Sequence*>(task_source()); }

   private:
    friend class Sequence;

    explicit Transaction(Sequence* sequence);
  };

  // This indicates where a sequence is stored, used by Sequence to keep track
  // of its status.
  enum class SequenceLocation {
    // Sequence is not present in any queue.
    kNone,
    // Sequence is present in queue of immediate sequences.
    kImmediateQueue,
    // Sequence is present in queue of delayed sequences.
    kDelayedQueue,
    // Sequence is being run by a worker.
    kInWorker,
  };

  // |traits| is metadata that applies to all Tasks in the Sequence.
  // |task_runner| is a reference to the TaskRunner feeding this TaskSource.
  // |task_runner| can be nullptr only for tasks with no TaskRunner, in which
  // case |execution_mode| must be kParallel. Otherwise, |execution_mode| is the
  // execution mode of |task_runner|.
  Sequence(const TaskTraits& traits,
           TaskRunner* task_runner,
           TaskSourceExecutionMode execution_mode);
  Sequence(const Sequence&) = delete;
  Sequence& operator=(const Sequence&) = delete;

  // Begins a Transaction. This method cannot be called on a thread which has an
  // active Sequence::Transaction.
  [[nodiscard]] Transaction BeginTransaction();

  // TaskSource:
  ExecutionEnvironment GetExecutionEnvironment() override;
  size_t GetRemainingConcurrency() const override;
  TaskSourceSortKey GetSortKey() const override;
  TimeTicks GetDelayedSortKey() const override;

  // Returns a token that uniquely identifies this Sequence.
  const SequenceToken& token() const { return token_; }

  SequenceLocalStorageMap* sequence_local_storage() {
    return &sequence_local_storage_;
  }

  SequenceLocation GetCurrentLocationForTesting();

  void OnBecomeReady() override;

 private:
  ~Sequence() override;

  struct DelayedTaskGreater {
    bool operator()(const Task& lhs, const Task& rhs) const;
  };

  // TaskSource:
  RunStatus WillRunTask() override;
  Task TakeTask(TaskSource::Transaction* transaction) override;
  Task Clear(TaskSource::Transaction* transaction) override;
  bool DidProcessTask(TaskSource::Transaction* transaction) override;
  bool WillReEnqueue(TimeTicks now,
                     TaskSource::Transaction* transaction) override;

  // Selects the earliest task to run, either from immediate or
  // delayed queue and return it.
  // Expects this sequence to have at least one task that can run
  // immediately.
  Task TakeEarliestTask();

  // Get and return next task from immediate queue
  Task TakeNextImmediateTask();

  // Determine next ready time and set ready time to it
  TimeTicks GetNextReadyTime();

  // Returns true if there are immediate tasks
  bool HasImmediateTasks() const;

  // Returns true if there are tasks ripe for execution in the delayed queue
  bool HasRipeDelayedTasks(TimeTicks now) const;

  // Returns true if tasks ready to be executed
  bool HasReadyTasks(TimeTicks now) const;

  bool IsEmpty() const;

  TimeTicks GetReadyTime() const;

  // Releases reference to TaskRunner.
  void ReleaseTaskRunner();

  const SequenceToken token_ = SequenceToken::Create();

  // Queues of tasks to execute.
  base::queue<Task> queue_;
  base::IntrusiveHeap<Task, DelayedTaskGreater> delayed_queue_;

  std::atomic<TimeTicks> ready_time_{TimeTicks()};

  // Holds data stored through the SequenceLocalStorageSlot API.
  SequenceLocalStorageMap sequence_local_storage_;

  // This member will hold the current location of the sequence at any time.
  // At instantiation, the sequence is not put in any queue yet so the
  // sequence location is set to |kNone|.
  std::atomic<SequenceLocation> current_location_{SequenceLocation::kNone};
};

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_THREAD_POOL_SEQUENCE_H_
