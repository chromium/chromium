// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/synchronization/waitable_event.h"

#include "base/check.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/trace_event/base_tracing.h"

namespace base {

void WaitableEvent::Signal() {
  // Must be ordered before SignalImpl() to guarantee it's emitted before the
  // matching TerminatingFlow in TimedWait().
  if (!only_used_while_idle_) {
    TRACE_EVENT_INSTANT("wakeup.flow,toplevel.flow", "WaitableEvent::Signal",
                        perfetto::Flow::FromPointer(this));
  }
  SignalImpl();
}

void WaitableEvent::Wait() {
  const bool result = TimedWait(TimeDelta::Max());
  DCHECK(result) << "TimedWait() should never fail with infinite timeout";
}

bool WaitableEvent::TimedWait(TimeDelta wait_delta) {
  if (wait_delta <= TimeDelta())
    return IsSignaled();

  // Consider this thread blocked for scheduling purposes. Ignore this for
  // non-blocking WaitableEvents.
  std::optional<internal::ScopedBlockingCallWithBaseSyncPrimitives>
      scoped_blocking_call;
  if (!only_used_while_idle_) {
    scoped_blocking_call.emplace(FROM_HERE, BlockingType::MAY_BLOCK);
  }

  const bool result = TimedWaitImpl(wait_delta);

  if (result && !only_used_while_idle_) {
    TRACE_EVENT_INSTANT("wakeup.flow,toplevel.flow",
                        "WaitableEvent::Wait Complete",
                        perfetto::TerminatingFlow::FromPointer(this));
  }

  return result;
}

size_t WaitableEvent::WaitMany(WaitableEvent** events, size_t count) {
  DCHECK(count) << "Cannot wait on no events";
  internal::ScopedBlockingCallWithBaseSyncPrimitives scoped_blocking_call(
      FROM_HERE, BlockingType::MAY_BLOCK);

  const size_t signaled_id = WaitManyImpl(events, count);
  WaitableEvent* const signaled_event = events[signaled_id];
  if (!signaled_event->only_used_while_idle_) {
    TRACE_EVENT_INSTANT("wakeup.flow,toplevel.flow",
                        "WaitableEvent::WaitMany Complete",
                        perfetto::TerminatingFlow::FromPointer(signaled_event));
  }
  return signaled_id;
}

}  // namespace base
