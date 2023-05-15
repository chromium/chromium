// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sequence_checker_impl.h"

#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/debug/stack_trace.h"
#include "base/memory/ptr_util.h"
#include "base/sequence_token.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_checker_impl.h"
#include "base/threading/thread_local_storage.h"

namespace base {

// static
void SequenceCheckerImpl::EnableStackLogging() {
  ThreadChecker::EnableStackLogging();
}

SequenceCheckerImpl::SequenceCheckerImpl() = default;
SequenceCheckerImpl::~SequenceCheckerImpl() = default;

SequenceCheckerImpl::SequenceCheckerImpl(SequenceCheckerImpl&& other) = default;
SequenceCheckerImpl& SequenceCheckerImpl::operator=(
    SequenceCheckerImpl&& other) = default;

bool SequenceCheckerImpl::CalledOnValidSequence(
    std::unique_ptr<debug::StackTrace>* bound_at) const {
  AutoLock auto_lock(thread_checker_.lock_);
  // When `sequence_token_` or SequenceToken::GetForCurrentThread() are
  // invalid fall back on ThreadChecker. We assume that SequenceChecker things
  // are mostly run on a sequence and that that is the correct sequence (hence
  // using LIKELY annotation).
  if (LIKELY(thread_checker_.sequence_token_.IsValid())) {
    if (LIKELY(thread_checker_.sequence_token_ ==
               SequenceToken::GetForCurrentThread())) {
      return true;
    }

    // TODO(pbos): This preserves existing behavior that `sequence_token_` is
    // ignored after TLS shutdown. It should either be documented here why
    // that is necessary (shouldn't this destroy on sequence?) or
    // SequenceCheckerTest.CalledOnValidSequenceFromThreadDestruction should
    // be updated to reflect the expected behavior.
    //
    // crrev.com/682023 added this TLS-check to solve an edge case but that
    // edge case was probably only a problem before TLS-destruction order was
    // fixed in crrev.com/1119244. crrev.com/1117059 further improved
    // TLS-destruction order of tokens by using `thread_local` and making it
    // deterministic.
    // See https://timsong-cpp.github.io/cppwp/n4140/basic.start.term: "If the
    // completion of the constructor or dynamic initialization of an object
    // with thread storage duration is sequenced before that of another, the
    // completion of the destructor of the second is sequenced before the
    // initiation of the destructor of the first."
    if (!ThreadLocalStorage::HasBeenDestroyed()) {
      if (bound_at) {
        *bound_at = thread_checker_.GetBoundAt();
      }
      return false;
    }
  }

  // SequenceChecker behaves as a ThreadChecker when it is not bound to a
  // valid sequence token.
  return thread_checker_.CalledOnValidThreadInternal(bound_at);
}

void SequenceCheckerImpl::DetachFromSequence() {
  thread_checker_.DetachFromThread();
}

}  // namespace base
