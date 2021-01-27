// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/thread_checker_impl.h"

#include "base/check.h"
#include "base/debug/stack_trace.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_task_runner_handle.h"

namespace {
bool g_log_thread_and_sequence_checker_binding = false;
}

namespace base {

// static
void ThreadCheckerImpl::EnableStackLogging() {
  g_log_thread_and_sequence_checker_binding = true;
}

ThreadCheckerImpl::ThreadCheckerImpl() {
  AutoLock auto_lock(lock_);
  EnsureAssignedLockRequired();
}

ThreadCheckerImpl::~ThreadCheckerImpl() = default;

ThreadCheckerImpl::ThreadCheckerImpl(ThreadCheckerImpl&& other) {
  // Verify that |other| is called on its associated thread and bind it now if
  // it is currently detached (even if this isn't a DCHECK build).
  const bool other_called_on_valid_thread = other.CalledOnValidThread();
  DCHECK(other_called_on_valid_thread);

  // Intentionally not using |other.lock_| to let TSAN catch racy construct from
  // |other|.
  bound_at_ = std::move(other.bound_at_);
  thread_id_ = other.thread_id_;
  task_token_ = other.task_token_;
  sequence_token_ = other.sequence_token_;

  // other.bound_at_ was moved from so it's null.
  other.thread_id_ = PlatformThreadRef();
  other.task_token_ = TaskToken();
  other.sequence_token_ = SequenceToken();
}

ThreadCheckerImpl& ThreadCheckerImpl::operator=(ThreadCheckerImpl&& other) {
  DCHECK(CalledOnValidThread());

  // Verify that |other| is called on its associated thread and bind it now if
  // it is currently detached (even if this isn't a DCHECK build).
  const bool other_called_on_valid_thread = other.CalledOnValidThread();
  DCHECK(other_called_on_valid_thread);

  // Intentionally not using either |lock_| to let TSAN catch racy assign.
  TS_UNCHECKED_READ(thread_id_) = TS_UNCHECKED_READ(other.thread_id_);
  TS_UNCHECKED_READ(task_token_) = TS_UNCHECKED_READ(other.task_token_);
  TS_UNCHECKED_READ(sequence_token_) = TS_UNCHECKED_READ(other.sequence_token_);

  TS_UNCHECKED_READ(other.thread_id_) = PlatformThreadRef();
  TS_UNCHECKED_READ(other.task_token_) = TaskToken();
  TS_UNCHECKED_READ(other.sequence_token_) = SequenceToken();

  return *this;
}

bool ThreadCheckerImpl::CalledOnValidThread(
    std::unique_ptr<debug::StackTrace>* out_bound_at) const {
  const bool has_thread_been_destroyed = ThreadLocalStorage::HasBeenDestroyed();

  AutoLock auto_lock(lock_);
  // TaskToken/SequenceToken access thread-local storage. During destruction
  // the state of thread-local storage is not guaranteed to be in a consistent
  // state. Further, task-runner only installs the tokens when running a task.
  if (!has_thread_been_destroyed) {
    EnsureAssignedLockRequired();

    // Always return true when called from the task from which this
    // ThreadCheckerImpl was assigned to a thread.
    if (task_token_ == TaskToken::GetForCurrentThread())
      return true;

    // If this ThreadCheckerImpl is bound to a valid SequenceToken, it must be
    // equal to the current SequenceToken and there must be a registered
    // ThreadTaskRunnerHandle. Otherwise, the fact that the current task runs on
    // the thread to which this ThreadCheckerImpl is bound is fortuitous.
    if (sequence_token_.IsValid() &&
        (sequence_token_ != SequenceToken::GetForCurrentThread() ||
         !ThreadTaskRunnerHandle::IsSet())) {
      if (out_bound_at && bound_at_) {
        *out_bound_at = std::make_unique<debug::StackTrace>(*bound_at_);
      }
      return false;
    }
  } else if (thread_id_.is_null()) {
    // We're in tls destruction but the |thread_id_| hasn't been assigned yet.
    // Assign it now. This doesn't call EnsureAssigned() as to do so while in
    // tls destruction may result in the wrong TaskToken/SequenceToken.
    if (g_log_thread_and_sequence_checker_binding)
      bound_at_ = std::make_unique<debug::StackTrace>(size_t{10});
    thread_id_ = PlatformThread::CurrentRef();
    return true;
  }

  if (thread_id_ != PlatformThread::CurrentRef()) {
    if (out_bound_at && bound_at_) {
      *out_bound_at = std::make_unique<debug::StackTrace>(*bound_at_);
    }
    return false;
  }
  return true;
}

void ThreadCheckerImpl::DetachFromThread() {
  AutoLock auto_lock(lock_);
  bound_at_ = nullptr;
  thread_id_ = PlatformThreadRef();
  task_token_ = TaskToken();
  sequence_token_ = SequenceToken();
}

std::unique_ptr<debug::StackTrace> ThreadCheckerImpl::GetBoundAt() const {
  AutoLock auto_lock(lock_);
  if (!bound_at_)
    return nullptr;
  return std::make_unique<debug::StackTrace>(*bound_at_);
}

void ThreadCheckerImpl::EnsureAssignedLockRequired() const {
  if (!thread_id_.is_null())
    return;
  if (g_log_thread_and_sequence_checker_binding)
    bound_at_ = std::make_unique<debug::StackTrace>(size_t{10});
  thread_id_ = PlatformThread::CurrentRef();
  task_token_ = TaskToken::GetForCurrentThread();
  sequence_token_ = SequenceToken::GetForCurrentThread();
}

}  // namespace base
