// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_TASK_SCHEDULER_SEQUENCE_H_
#define BASE_TASK_TASK_SCHEDULER_SEQUENCE_H_

#include <stddef.h>

#include "base/base_export.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/sequence_token.h"
#include "base/task/task_scheduler/scheduler_lock.h"
#include "base/task/task_scheduler/sequence_sort_key.h"
#include "base/task/task_scheduler/task.h"
#include "base/task/task_traits.h"
#include "base/threading/sequence_local_storage_map.h"

namespace base {
namespace internal {

// A Sequence holds slots each containing up to a single Task that must be
// executed in posting order.
//
// In comments below, an "empty Sequence" is a Sequence with no slot.
//
// Note: there is a known refcounted-ownership cycle in the Scheduler
// architecture: Sequence -> Task -> TaskRunner -> Sequence -> ...
// This is okay so long as the other owners of Sequence (PriorityQueue and
// SchedulerWorker in alternation and
// SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl::GetWork()
// temporarily) keep running it (and taking Tasks from it as a result). A
// dangling reference cycle would only occur should they release their reference
// to it while it's not empty. In other words, it is only correct for them to
// release it after PopTask() returns false to indicate it was made empty by
// that call (in which case the next PushTask() will return true to indicate to
// the caller that the Sequence should be re-enqueued for execution).
//
// This class is thread-safe.
class BASE_EXPORT Sequence : public RefCountedThreadSafe<Sequence> {
 public:
  // |traits| is metadata that applies to all Tasks in the Sequence.
  explicit Sequence(const TaskTraits& traits);

  // Adds |task| in a new slot at the end of the Sequence. Returns true if the
  // Sequence was empty before this operation.
  bool PushTask(Task task);

  // Transfers ownership of the Task in the front slot of the Sequence to the
  // caller. The front slot of the Sequence will be nullptr and remain until
  // Pop(). Cannot be called on an empty Sequence or a Sequence whose front slot
  // is already nullptr.
  //
  // Because this method cannot be called on an empty Sequence, the returned
  // Optional<Task> is never nullptr. An Optional is used in preparation for the
  // merge between TaskScheduler and TaskQueueManager (in Blink).
  // https://crbug.com/783309
  Optional<Task> TakeTask();

  // Removes the front slot of the Sequence. The front slot must have been
  // emptied by TakeTask() before this is called. Cannot be called on an empty
  // Sequence. Returns true if the Sequence is empty after this operation.
  bool Pop();

  // Returns a SequenceSortKey representing the priority of the Sequence. Cannot
  // be called on an empty Sequence.
  SequenceSortKey GetSortKey() const;

  // Returns a token that uniquely identifies this Sequence.
  const SequenceToken& token() const { return token_; }

  SequenceLocalStorageMap* sequence_local_storage() {
    return &sequence_local_storage_;
  }

  // Returns the TaskTraits for all Tasks in the Sequence.
  TaskTraits traits() const { return traits_; }

 private:
  friend class RefCountedThreadSafe<Sequence>;
  ~Sequence();

  const SequenceToken token_ = SequenceToken::Create();

  // Synchronizes access to all members.
  mutable SchedulerLock lock_;

  // Queue of tasks to execute.
  base::queue<Task> queue_;

  // Holds data stored through the SequenceLocalStorageSlot API.
  SequenceLocalStorageMap sequence_local_storage_;

  // The TaskTraits of all Tasks in the Sequence.
  const TaskTraits traits_;

  DISALLOW_COPY_AND_ASSIGN(Sequence);
};

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_TASK_SCHEDULER_SEQUENCE_H_
