// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_THREAD_POOL_WORKER_THREAD_WAITABLE_EVENT_H_
#define BASE_TASK_THREAD_POOL_WORKER_THREAD_WAITABLE_EVENT_H_

#include "base/task/thread_pool/worker_thread.h"

#include <memory>

#include "base/base_export.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/common/checked_lock.h"
#include "base/task/thread_pool/tracked_ref.h"

namespace base {

namespace internal {

class BASE_EXPORT WorkerThreadWaitableEvent : public WorkerThread {
 public:
  class BASE_EXPORT Delegate : public WorkerThread::Delegate {
   protected:
    friend WorkerThreadWaitableEvent;
    bool TimedWait(TimeDelta timeout) override;
    // Event to wake up the thread managed by the WorkerThread whose delegate
    // this is.
    WaitableEvent wake_up_event_{WaitableEvent::ResetPolicy::AUTOMATIC,
                                 WaitableEvent::InitialState::NOT_SIGNALED};
  };

  // Everything is passed to WorkerThread's constructor, except the Delegate.
  WorkerThreadWaitableEvent(ThreadType thread_type_hint,
                            std::unique_ptr<Delegate> delegate,
                            TrackedRef<TaskTracker> task_tracker,
                            size_t sequence_num,
                            const CheckedLock* predecessor_lock = nullptr);

  WorkerThreadWaitableEvent(const WorkerThread&) = delete;
  WorkerThreadWaitableEvent& operator=(const WorkerThread&) = delete;

  // Wakes up this WorkerThreadWaitableEvent if it wasn't already awake. After
  // this is called, this WorkerThreadWaitableEvent will run Tasks from
  // TaskSources returned by the GetWork() method of its delegate until it
  // returns nullptr. No-op if Start() wasn't called. DCHECKs if called after
  // Start() has failed or after Cleanup() has been called.
  void WakeUp();

  // WorkerThread:
  void JoinForTesting() override;
  void Cleanup() override;
  Delegate* delegate() override;

 private:
  const std::unique_ptr<Delegate> delegate_;

  ~WorkerThreadWaitableEvent() override;
};

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_THREAD_POOL_WORKER_THREAD_WAITABLE_EVENT_H_
