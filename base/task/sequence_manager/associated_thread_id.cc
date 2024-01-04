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
  const auto prev_thread_ref = thread_ref_.load(std::memory_order_relaxed);
  DCHECK(prev_thread_ref.is_null() ||
         prev_thread_ref == PlatformThread::CurrentRef());
#endif
  sequence_token_ = base::internal::SequenceToken::GetForCurrentThread();
  thread_ref_.store(PlatformThread::CurrentRef(), std::memory_order_release);

  // Rebind the thread and sequence checkers to the current thread/sequence.
  DETACH_FROM_THREAD(thread_checker);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker);

  DETACH_FROM_SEQUENCE(sequence_checker);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker);
}

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base
