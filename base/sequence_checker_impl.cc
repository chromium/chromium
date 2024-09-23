// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sequence_checker_impl.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/contains.h"
#include "base/debug/stack_trace.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_token.h"
#include "base/synchronization/lock_subtle.h"
#include "base/threading/platform_thread.h"
#include "base/threading/platform_thread_ref.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_checker_impl.h"
#include "base/threading/thread_local_storage.h"

namespace base {

namespace {
bool g_log_stack = false;
}

// static
void SequenceCheckerImpl::EnableStackLogging() {
  g_log_stack = true;
  ThreadChecker::EnableStackLogging();
}

SequenceCheckerImpl::SequenceCheckerImpl() {
  AutoLock auto_lock(lock_);
  EnsureAssigned();
}

SequenceCheckerImpl::~SequenceCheckerImpl() = default;

SequenceCheckerImpl::SequenceCheckerImpl(SequenceCheckerImpl&& other) {
  // Verify that `other` is called on the correct thread.
  // Note: This binds `other` if not already bound.
  CHECK(other.CalledOnValidSequence());

  bound_at_ = std::move(other.bound_at_);
  sequence_token_ = other.sequence_token_;
#if DCHECK_IS_ON()
  locks_ = std::move(other.locks_);
#endif  // DCHECK_IS_ON()
  thread_ref_ = other.thread_ref_;

  // `other.bound_at_` and `other.locks_` were moved so they're null.
  DCHECK(!other.bound_at_);
#if DCHECK_IS_ON()
  DCHECK(other.locks_.empty());
#endif  // DCHECK_IS_ON()
  other.sequence_token_ = internal::SequenceToken();
  other.thread_ref_ = PlatformThreadRef();
}

SequenceCheckerImpl& SequenceCheckerImpl::operator=(
    SequenceCheckerImpl&& other) {
  // Verify that `other` is called on the correct thread.
  // Note: This binds `other` if not already bound.
  CHECK(other.CalledOnValidSequence());

  TS_UNCHECKED_READ(bound_at_) = std::move(TS_UNCHECKED_READ(other.bound_at_));
  TS_UNCHECKED_READ(sequence_token_) = TS_UNCHECKED_READ(other.sequence_token_);
#if DCHECK_IS_ON()
  TS_UNCHECKED_READ(locks_) = std::move(TS_UNCHECKED_READ(other.locks_));
#endif  // DCHECK_IS_ON()
  TS_UNCHECKED_READ(thread_ref_) = TS_UNCHECKED_READ(other.thread_ref_);

  // `other.bound_at_` and `other.locks_` were moved so they're null.
  DCHECK(!TS_UNCHECKED_READ(other.bound_at_));
#if DCHECK_IS_ON()
  DCHECK(TS_UNCHECKED_READ(other.locks_).empty());
#endif  // DCHECK_IS_ON()
  TS_UNCHECKED_READ(other.sequence_token_) = internal::SequenceToken();
  TS_UNCHECKED_READ(other.thread_ref_) = PlatformThreadRef();

  return *this;
}

bool SequenceCheckerImpl::CalledOnValidSequence(
    std::unique_ptr<debug::StackTrace>* out_bound_at) const {
  AutoLock auto_lock(lock_);
  EnsureAssigned();
  CHECK(!thread_ref_.is_null());

  // Valid if current sequence is the bound sequence.
  bool is_valid =
      (sequence_token_ == internal::SequenceToken::GetForCurrentThread());

  // Valid if holding a bound lock.
  if (!is_valid) {
#if DCHECK_IS_ON()
    for (uintptr_t lock : subtle::GetTrackedLocksHeldByCurrentThread()) {
      if (Contains(locks_, lock)) {
        is_valid = true;
        break;
      }
    }
#endif  // DCHECK_IS_ON()
  }

  // Valid if called from the bound thread after TLS destruction.
  //
  // TODO(pbos): This preserves existing behavior that `sequence_token_` is
  // ignored after TLS shutdown. It should either be documented here why that is
  // necessary (shouldn't this destroy on sequence?) or
  // SequenceCheckerTest.FromThreadDestruction should be updated to reflect the
  // expected behavior.
  //
  // crrev.com/682023 added this TLS-check to solve an edge case but that edge
  // case was probably only a problem before TLS-destruction order was fixed in
  // crrev.com/1119244. crrev.com/1117059 further improved TLS-destruction order
  // of tokens by using `thread_local` and making it deterministic.
  //
  // See https://timsong-cpp.github.io/cppwp/n4140/basic.start.term: "If the
  // completion of the constructor or dynamic initialization of an object with
  // thread storage duration is sequenced before that of another, the completion
  // of the destructor of the second is sequenced before the initiation of the
  // destructor of the first."
  if (!is_valid) {
    is_valid = ThreadLocalStorage::HasBeenDestroyed() &&
               thread_ref_ == PlatformThread::CurrentRef();
  }

  if (!is_valid) {
    // Return false without modifying the state if this call is not guaranteed
    // to be mutually exclusive with others that returned true. Not modifying
    // the state allows future calls to return true if they are mutually
    // exclusive with other calls that returned true.

    // On failure, set the `out_bound_at` argument.
    if (out_bound_at && bound_at_) {
      *out_bound_at = std::make_unique<debug::StackTrace>(*bound_at_);
    }

    return false;
  }

  // Before returning true, modify the state so future calls only return true if
  // they are guaranteed to be mutually exclusive with this one.

#if DCHECK_IS_ON()
  // `locks_` must contain locks held at binding time and for all calls to
  // `CalledOnValidSequence` that returned true afterwards.
  std::erase_if(locks_, [](uintptr_t lock_ptr) {
    return !Contains(subtle::GetTrackedLocksHeldByCurrentThread(), lock_ptr);
  });
#endif  // DCHECK_IS_ON()

  // `sequence_token_` is reset if this returns true from a different sequence.
  if (sequence_token_ != internal::SequenceToken::GetForCurrentThread()) {
    sequence_token_ = internal::SequenceToken();
  }

  return true;
}

void SequenceCheckerImpl::DetachFromSequence() {
  AutoLock auto_lock(lock_);
  bound_at_.reset();
  sequence_token_ = internal::SequenceToken();
#if DCHECK_IS_ON()
  locks_.clear();
#endif  // DCHECK_IS_ON()
  thread_ref_ = PlatformThreadRef();
}

void SequenceCheckerImpl::EnsureAssigned() const {
  // Use `thread_ref_` to determine if this checker is already bound, as it is
  // always set when bound (unlike `sequence_token_` and `locks_` which may be
  // cleared by `CalledOnValidSequence()` while this checker is still bound).
  if (!thread_ref_.is_null()) {
    return;
  }

  if (g_log_stack) {
    bound_at_ = std::make_unique<debug::StackTrace>(size_t{10});
  }

  sequence_token_ = internal::SequenceToken::GetForCurrentThread();

#if DCHECK_IS_ON()
  // Copy all held locks to `locks_`, except `&lock_` (this is an implementation
  // detail of `SequenceCheckerImpl` and doesn't provide mutual exclusion
  // guarantees to the caller).
  DCHECK(locks_.empty());
  ranges::remove_copy(subtle::GetTrackedLocksHeldByCurrentThread(),
                      std::back_inserter(locks_),
                      reinterpret_cast<uintptr_t>(&lock_));
#endif  // DCHECK_IS_ON()

  DCHECK(sequence_token_.IsValid());
  thread_ref_ = PlatformThread::CurrentRef();
  DCHECK(!thread_ref_.is_null());
}

}  // namespace base
