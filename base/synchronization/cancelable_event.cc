// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "base/synchronization/cancelable_event.h"

#include "base/notreached.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/trace_event/base_tracing.h"
#include "build/build_config.h"

namespace base {

void CancelableEvent::Signal() {
#if BUILDFLAG(ENABLE_BASE_TRACING)
  // Must be ordered before SignalImpl() to match the `TerminatingFlow` in
  // TimedWait() and Cancel().
  if (!only_used_while_idle_) {
    TRACE_EVENT_INSTANT("wakeup.flow,toplevel.flow", "CancelableEvent::Signal",
                        perfetto::Flow::FromPointer(this));
  }
#endif
  SignalImpl();
}

bool CancelableEvent::Cancel() {
#if BUILDFLAG(ENABLE_BASE_TRACING)
  if (!only_used_while_idle_) {
    TRACE_EVENT_INSTANT("wakeup.flow,toplevel.flow", "CancelableEvent::Cancel",
                        perfetto::TerminatingFlow::FromPointer(this));
  }
#endif
  return CancelImpl();
}

bool CancelableEvent::TimedWait(TimeDelta timeout) {
  // Consider this thread blocked for scheduling purposes. Ignore this for
  // non-blocking CancelableEvents.
  std::optional<internal::ScopedBlockingCallWithBaseSyncPrimitives>
      scoped_blocking_call;
  if (!only_used_while_idle_) {
    scoped_blocking_call.emplace(FROM_HERE, BlockingType::MAY_BLOCK);
  }

  const bool result = TimedWaitImpl(timeout);

#if BUILDFLAG(ENABLE_BASE_TRACING)
  if (result && !only_used_while_idle_) {
    TRACE_EVENT_INSTANT("wakeup.flow,toplevel.flow",
                        "CancelableEvent::Wait Complete",
                        perfetto::TerminatingFlow::FromPointer(this));
  }
#endif

  return result;
}

}  // namespace base
