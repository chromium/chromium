// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/associated_thread_id.h"

#include "base/check.h"
#include "base/dcheck_is_on.h"

namespace base {
namespace sequence_manager {
namespace internal {

AssociatedThreadId::AssociatedThreadId() = default;
AssociatedThreadId::~AssociatedThreadId() = default;

void AssociatedThreadId::BindToCurrentThread() {
#if DCHECK_IS_ON()
  const auto prev_thread_ref =
      bound_thread_ref_.load(std::memory_order_relaxed);
  DCHECK(prev_thread_ref.is_null() ||
         prev_thread_ref == PlatformThread::CurrentRef());
#endif
  sequence_token_ = base::internal::SequenceToken::GetForCurrentThread();
  bound_thread_ref_.store(PlatformThread::CurrentRef(),
                          std::memory_order_release);

  // Rebind the thread and sequence checkers to the current thread/sequence.
  DETACH_FROM_THREAD(thread_checker);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker);

  DETACH_FROM_SEQUENCE(sequence_checker);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker);
}

bool AssociatedThreadId::IsBoundToCurrentThread() const {
  const PlatformThreadRef bound_thread_ref =
      bound_thread_ref_.load(std::memory_order_relaxed);
  const PlatformThreadRef in_sequence_thread_ref =
      in_sequence_thread_ref_.load(std::memory_order_relaxed);
  const PlatformThreadRef current_thread_ref = PlatformThread::CurrentRef();

  if (!in_sequence_thread_ref.is_null()) {
    // The main thread cannot run when another thread is running "in sequence"
    // with it.
    CHECK_NE(current_thread_ref, bound_thread_ref);
  }

  return bound_thread_ref == current_thread_ref;
}

void AssociatedThreadId::AssertInSequenceWithCurrentThread() const {
  const PlatformThreadRef in_sequence_thread_ref =
      in_sequence_thread_ref_.load(std::memory_order_relaxed);

  if (!in_sequence_thread_ref.is_null()) {
    CHECK_EQ(in_sequence_thread_ref, PlatformThread::CurrentRef());
    return;
  }

#if DCHECK_IS_ON()
  const PlatformThreadRef bound_thread_ref =
      bound_thread_ref_.load(std::memory_order_relaxed);
  if (!bound_thread_ref.is_null()) {
    CHECK_EQ(bound_thread_ref, PlatformThread::CurrentRef());
    return;
  }

  DCHECK_CALLED_ON_VALID_THREAD(thread_checker);
#endif  // DCHECK_IS_ON()
}

void AssociatedThreadId::StartInSequenceWithCurrentThread() {
  PlatformThreadRef expected = PlatformThreadRef();
  bool succeeded = in_sequence_thread_ref_.compare_exchange_strong(
      expected, PlatformThread::CurrentRef(), std::memory_order_relaxed);
  CHECK(succeeded);
}

void AssociatedThreadId::StopInSequenceWithCurrentThread() {
  PlatformThreadRef expected = PlatformThread::CurrentRef();
  bool succeeded = in_sequence_thread_ref_.compare_exchange_strong(
      expected, PlatformThreadRef(), std::memory_order_relaxed);
  CHECK(succeeded);
}

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base
