// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/thread_checker_impl.h"

#include "base/check.h"
#include "base/debug/stack_trace.h"
#include "base/sequence_token.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_local.h"

namespace {
bool g_log_stack = false;
}

namespace base {

// static
void ThreadCheckerImpl::EnableStackLogging() {
  g_log_stack = true;
}

ThreadCheckerImpl::ThreadCheckerImpl() {
  AutoLock auto_lock(lock_);
  EnsureAssigned();
}

ThreadCheckerImpl::~ThreadCheckerImpl() = default;

ThreadCheckerImpl::ThreadCheckerImpl(ThreadCheckerImpl&& other) {
  // Verify that `other` is called on the correct thread.
  // Note: This binds `other` if not already bound.
  CHECK(other.CalledOnValidThread());

  //  Not using `other.lock_` to let TSAN catch racy construct from `other`.
  bound_at_ = std::move(other.bound_at_);
  thread_ref_ = other.thread_ref_;
  task_token_ = other.task_token_;
  sequence_token_ = other.sequence_token_;

  // `other.bound_at_` was moved from so it's null.
  other.thread_ref_ = PlatformThreadRef();
  other.task_token_ = internal::TaskToken();
  other.sequence_token_ = internal::SequenceToken();
}

ThreadCheckerImpl& ThreadCheckerImpl::operator=(ThreadCheckerImpl&& other) {
  CHECK(CalledOnValidThread());

  // Verify that `other` is called on the correct thread.
  // Note: This binds `other` if not already bound.
  CHECK(other.CalledOnValidThread());

  // Intentionally not using either |lock_| to let TSAN catch racy assign.
  TS_UNCHECKED_READ(thread_ref_) = TS_UNCHECKED_READ(other.thread_ref_);
  TS_UNCHECKED_READ(task_token_) = TS_UNCHECKED_READ(other.task_token_);
  TS_UNCHECKED_READ(sequence_token_) = TS_UNCHECKED_READ(other.sequence_token_);

  TS_UNCHECKED_READ(other.thread_ref_) = PlatformThreadRef();
  TS_UNCHECKED_READ(other.task_token_) = internal::TaskToken();
  TS_UNCHECKED_READ(other.sequence_token_) = internal::SequenceToken();

  return *this;
}

bool ThreadCheckerImpl::CalledOnValidThread(
    std::unique_ptr<debug::StackTrace>* out_bound_at) const {
  AutoLock auto_lock(lock_);
  // If we're detached, bind to current state.
  EnsureAssigned();
  DCHECK(sequence_token_.IsValid());

  // Cases to handle:
  //
  // 1. Bound outside a task and used on the same thread: return true.
  // 2. Used on the same thread, TLS destroyed: return true.
  //         Note: This case exists for historical reasons and should be
  //         removed. See details in `SequenceCheckerImpl`.
  // 3. Same sequence as when this was bound:
  //   3a. Sequence is associated with a thread: return true.
  //   3b. Sequence may run on any thread: return false.
  //         Note: Return false even if this happens on the same thread as when
  //         this was bound, because that would be fortuitous.
  // 4. Different sequence than when this was bound: return false.

  if (thread_ref_ == PlatformThread::CurrentRef()) {
    // If this runs on the bound thread:

    // Return true if the checker was bound outside of a `TaskScope`.
    if (!task_token_.IsValid()) {
      return true;
    }

    // Return true if the checker was bound in the same `TaskScope`.
    if (task_token_ == internal::TaskToken::GetForCurrentThread()) {
      return true;
    }

    // Return true if TLS has been destroyed.
    //
    // This exists for historical reasons and can probably be removed. See
    // details in `SequenceCheckerImpl::CalledOnValidSequence()`.
    if (ThreadLocalStorage::HasBeenDestroyed()) {
      return true;
    }

    // Return true if the checker was bound in the same thread-bound sequence.
    // `CurrentTaskIsThreadBound()` avoids returning true when non-thread-bound
    // tasks from the same sequence run on the same thread by chance.
    if (sequence_token_ == internal::SequenceToken::GetForCurrentThread() &&
        internal::CurrentTaskIsThreadBound()) {
      return true;
    }
  }

  // On failure, set the `out_bound_at` argument.
  if (out_bound_at && bound_at_) {
    *out_bound_at = std::make_unique<debug::StackTrace>(*bound_at_);
  }
  return false;
}

void ThreadCheckerImpl::DetachFromThread() {
  AutoLock auto_lock(lock_);
  bound_at_ = nullptr;
  thread_ref_ = PlatformThreadRef();
  task_token_ = internal::TaskToken();
  sequence_token_ = internal::SequenceToken();
}

void ThreadCheckerImpl::EnsureAssigned() const {
  if (!thread_ref_.is_null()) {
    return;
  }
  if (g_log_stack) {
    bound_at_ = std::make_unique<debug::StackTrace>(size_t{10});
  }
  thread_ref_ = PlatformThread::CurrentRef();
  task_token_ = internal::TaskToken::GetForCurrentThread();
  sequence_token_ = internal::SequenceToken::GetForCurrentThread();
}

}  // namespace base
