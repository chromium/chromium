// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sequence_checker_impl.h"

#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/debug/stack_trace.h"
#include "base/sequence_token.h"
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
  thread_ref_ = other.thread_ref_;

  // `other.bound_at_` was moved from so it's null.
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
  TS_UNCHECKED_READ(thread_ref_) = TS_UNCHECKED_READ(other.thread_ref_);

  // `other.bound_at_` was moved from so it's null.
  TS_UNCHECKED_READ(other.sequence_token_) = internal::SequenceToken();
  TS_UNCHECKED_READ(other.thread_ref_) = PlatformThreadRef();

  return *this;
}

bool SequenceCheckerImpl::CalledOnValidSequence(
    std::unique_ptr<debug::StackTrace>* out_bound_at) const {
  AutoLock auto_lock(lock_);
  // If we're detached, bind to current state.
  EnsureAssigned();

  CHECK(!thread_ref_.is_null());

  // Return true if called from the bound sequence.
  if (sequence_token_ == internal::SequenceToken::GetForCurrentThread()) {
    return true;
  }

  // Return true if called from the bound thread after TLS destruction.
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
  if (ThreadLocalStorage::HasBeenDestroyed() &&
      thread_ref_ == PlatformThread::CurrentRef()) {
    return true;
  }

  // On failure, set the `out_bound_at` argument.
  if (out_bound_at && bound_at_) {
    *out_bound_at = std::make_unique<debug::StackTrace>(*bound_at_);
  }
  return false;
}

void SequenceCheckerImpl::DetachFromSequence() {
  AutoLock auto_lock(lock_);
  bound_at_.reset();
  sequence_token_ = internal::SequenceToken();
  thread_ref_ = PlatformThreadRef();
}

void SequenceCheckerImpl::EnsureAssigned() const {
  if (sequence_token_.IsValid()) {
    return;
  }

  if (g_log_stack) {
    bound_at_ = std::make_unique<debug::StackTrace>(size_t{10});
  }

  sequence_token_ = internal::SequenceToken::GetForCurrentThread();
  DCHECK(sequence_token_.IsValid());
  thread_ref_ = PlatformThread::CurrentRef();
  DCHECK(!thread_ref_.is_null());
}

}  // namespace base
