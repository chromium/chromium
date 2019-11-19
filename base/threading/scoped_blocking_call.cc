// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/scoped_blocking_call.h"

#include "base/lazy_instance.h"
#include "base/scoped_clear_last_error.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"

namespace base {

namespace {

LazyInstance<ThreadLocalPointer<internal::BlockingObserver>>::Leaky
    tls_blocking_observer = LAZY_INSTANCE_INITIALIZER;

// Last ScopedBlockingCall instantiated on this thread.
LazyInstance<ThreadLocalPointer<internal::UncheckedScopedBlockingCall>>::Leaky
    tls_last_scoped_blocking_call = LAZY_INSTANCE_INITIALIZER;

#if DCHECK_IS_ON()
// Used to verify that the trace events used in the constructor do not result in
// instantiating a ScopedBlockingCall themselves (which would cause an infinite
// reentrancy loop).
LazyInstance<ThreadLocalBoolean>::Leaky tls_construction_in_progress =
    LAZY_INSTANCE_INITIALIZER;
#endif

}  // namespace

namespace internal {

UncheckedScopedBlockingCall::UncheckedScopedBlockingCall(
    BlockingType blocking_type)
    : blocking_observer_(tls_blocking_observer.Get().Get()),
      previous_scoped_blocking_call_(tls_last_scoped_blocking_call.Get().Get()),
      is_will_block_(blocking_type == BlockingType::WILL_BLOCK ||
                     (previous_scoped_blocking_call_ &&
                      previous_scoped_blocking_call_->is_will_block_)) {
  tls_last_scoped_blocking_call.Get().Set(this);

  if (blocking_observer_) {
    if (!previous_scoped_blocking_call_) {
      blocking_observer_->BlockingStarted(blocking_type);
    } else if (blocking_type == BlockingType::WILL_BLOCK &&
               !previous_scoped_blocking_call_->is_will_block_) {
      blocking_observer_->BlockingTypeUpgraded();
    }
  }
}

UncheckedScopedBlockingCall::~UncheckedScopedBlockingCall() {
  // TLS affects result of GetLastError() on Windows. ScopedClearLastError
  // prevents side effect.
  base::internal::ScopedClearLastError save_last_error;
  DCHECK_EQ(this, tls_last_scoped_blocking_call.Get().Get());
  tls_last_scoped_blocking_call.Get().Set(previous_scoped_blocking_call_);
  if (blocking_observer_ && !previous_scoped_blocking_call_)
    blocking_observer_->BlockingEnded();
}

}  // namespace internal

ScopedBlockingCall::ScopedBlockingCall(const Location& from_here,
                                       BlockingType blocking_type)
    : UncheckedScopedBlockingCall(blocking_type) {
#if DCHECK_IS_ON()
  DCHECK(!tls_construction_in_progress.Get().Get());
  tls_construction_in_progress.Get().Set(true);
#endif

  internal::AssertBlockingAllowed();
  TRACE_EVENT_BEGIN2("base", "ScopedBlockingCall", "file_name",
                     from_here.file_name(), "function_name",
                     from_here.function_name());
#if DCHECK_IS_ON()
  tls_construction_in_progress.Get().Set(false);
#endif
}

ScopedBlockingCall::~ScopedBlockingCall() {
  TRACE_EVENT_END0("base", "ScopedBlockingCall");
}

namespace internal {

ScopedBlockingCallWithBaseSyncPrimitives::
    ScopedBlockingCallWithBaseSyncPrimitives(BlockingType blocking_type)
    : ScopedBlockingCallWithBaseSyncPrimitives(FROM_HERE, blocking_type) {}

ScopedBlockingCallWithBaseSyncPrimitives::
    ScopedBlockingCallWithBaseSyncPrimitives(const Location& from_here,
                                             BlockingType blocking_type)
    : UncheckedScopedBlockingCall(blocking_type) {
#if DCHECK_IS_ON()
  DCHECK(!tls_construction_in_progress.Get().Get());
  tls_construction_in_progress.Get().Set(true);
#endif

  internal::AssertBaseSyncPrimitivesAllowed();
  TRACE_EVENT_BEGIN2("base", "ScopedBlockingCallWithBaseSyncPrimitives",
                     "file_name", from_here.file_name(), "function_name",
                     from_here.function_name());

#if DCHECK_IS_ON()
  tls_construction_in_progress.Get().Set(false);
#endif
}

ScopedBlockingCallWithBaseSyncPrimitives::
    ~ScopedBlockingCallWithBaseSyncPrimitives() {
  TRACE_EVENT_END0("base", "ScopedBlockingCallWithBaseSyncPrimitives");
}

void SetBlockingObserverForCurrentThread(BlockingObserver* blocking_observer) {
  DCHECK(!tls_blocking_observer.Get().Get());
  tls_blocking_observer.Get().Set(blocking_observer);
}

void ClearBlockingObserverForCurrentThread() {
  tls_blocking_observer.Get().Set(nullptr);
}

ScopedClearBlockingObserverForTesting::ScopedClearBlockingObserverForTesting()
    : blocking_observer_(tls_blocking_observer.Get().Get()) {
  tls_blocking_observer.Get().Set(nullptr);
}

ScopedClearBlockingObserverForTesting::
    ~ScopedClearBlockingObserverForTesting() {
  DCHECK(!tls_blocking_observer.Get().Get());
  tls_blocking_observer.Get().Set(blocking_observer_);
}

}  // namespace internal

}  // namespace base
