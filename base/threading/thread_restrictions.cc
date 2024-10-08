// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/thread_restrictions.h"

#include "base/check.h"
#include "base/threading/hang_watcher.h"
#include "base/trace_event/base_tracing.h"
#include "build/build_config.h"

namespace base {

BooleanWithOptionalStack::BooleanWithOptionalStack(bool value) : value_(value) {
#if CAPTURE_THREAD_RESTRICTIONS_STACK_TRACES()
  stack_.emplace();
#endif  // CAPTURE_THREAD_RESTRICTIONS_STACK_TRACES()
}

std::ostream& operator<<(std::ostream& out,
                         const BooleanWithOptionalStack& bws) {
  out << bws.value_;
#if CAPTURE_THREAD_RESTRICTIONS_STACK_TRACES()
  if (bws.stack_.has_value()) {
    out << " set by\n" << bws.stack_.value();
  } else {
    out << " (value by default)";
  }
#endif  // CAPTURE_THREAD_RESTRICTIONS_STACK_TRACES()
  return out;
}

// A macro that dumps in official builds (non-fatal) if the condition is false,
// or behaves as DCHECK in DCHECK-enabled builds. Unlike DUMP_WILL_BE_CHECK,
// there is no intent to transform those into CHECKs. Used to report potential
// performance issues.
//
// TODO(crbug.com/363049758): This is temporarily a `DCHECK` to avoid getting a
// lot of crash reports while known issues are being addressed. Change to
// `DUMP_WILL_BE_CHECK` once known issues are addressed.
#define DUMP_OR_DCHECK DCHECK

namespace {

constinit thread_local BooleanWithOptionalStack tls_blocking_disallowed;
constinit thread_local BooleanWithOptionalStack tls_singleton_disallowed;
constinit thread_local BooleanWithOptionalStack
    tls_base_sync_primitives_disallowed;
constinit thread_local BooleanWithOptionalStack
    tls_cpu_intensive_work_disallowed;

}  // namespace

namespace internal {

void AssertBlockingAllowed() {
  DUMP_OR_DCHECK(!tls_blocking_disallowed)
      << "Function marked as blocking was called from a scope that disallows "
         "blocking! If this task is running inside the ThreadPool, it needs "
         "to have MayBlock() in its TaskTraits. Otherwise, consider making "
         "this blocking work asynchronous or, as a last resort, you may use "
         "ScopedAllowBlocking (see its documentation for best practices).\n"
      << "tls_blocking_disallowed " << tls_blocking_disallowed;
}

void AssertBlockingDisallowedForTesting() {
  DCHECK(tls_blocking_disallowed)
      << "tls_blocking_disallowed " << tls_blocking_disallowed;
}

}  // namespace internal

void DisallowBlocking() {
  tls_blocking_disallowed = BooleanWithOptionalStack(true);
}

ScopedDisallowBlocking::ScopedDisallowBlocking()
    : resetter_(&tls_blocking_disallowed, BooleanWithOptionalStack(true)) {}

ScopedDisallowBlocking::~ScopedDisallowBlocking() {
  DCHECK(tls_blocking_disallowed)
      << "~ScopedDisallowBlocking() running while surprisingly already no "
         "longer disallowed.\n"
      << "tls_blocking_disallowed " << tls_blocking_disallowed;
}

void DisallowBaseSyncPrimitives() {
  tls_base_sync_primitives_disallowed = BooleanWithOptionalStack(true);
}

ScopedDisallowBaseSyncPrimitives::ScopedDisallowBaseSyncPrimitives()
    : resetter_(&tls_base_sync_primitives_disallowed,
                BooleanWithOptionalStack(true)) {}

ScopedDisallowBaseSyncPrimitives::~ScopedDisallowBaseSyncPrimitives() {
  DCHECK(tls_base_sync_primitives_disallowed)
      << "~ScopedDisallowBaseSyncPrimitives() running while surprisingly "
         "already no longer disallowed.\n"
      << "tls_base_sync_primitives_disallowed "
      << tls_base_sync_primitives_disallowed;
}

ScopedAllowBaseSyncPrimitives::ScopedAllowBaseSyncPrimitives()
    : resetter_(&tls_base_sync_primitives_disallowed,
                BooleanWithOptionalStack(false)) {
  DCHECK(!tls_blocking_disallowed)
      << "To allow //base sync primitives in a scope where blocking is "
         "disallowed use ScopedAllowBaseSyncPrimitivesOutsideBlockingScope.\n"
      << "tls_blocking_disallowed " << tls_blocking_disallowed;
}

ScopedAllowBaseSyncPrimitives::~ScopedAllowBaseSyncPrimitives() {
  DCHECK(!tls_base_sync_primitives_disallowed)
      << "~ScopedAllowBaseSyncPrimitives() running while surprisingly already "
         "no longer allowed.\n"
      << "tls_base_sync_primitives_disallowed "
      << tls_base_sync_primitives_disallowed;
}

ScopedAllowBaseSyncPrimitivesForTesting::
    ScopedAllowBaseSyncPrimitivesForTesting()
    : resetter_(&tls_base_sync_primitives_disallowed,
                BooleanWithOptionalStack(false)) {}

ScopedAllowBaseSyncPrimitivesForTesting::
    ~ScopedAllowBaseSyncPrimitivesForTesting() {
  DCHECK(!tls_base_sync_primitives_disallowed)
      << "~ScopedAllowBaseSyncPrimitivesForTesting() running while "  // IN-TEST
         "surprisingly already no longer allowed.\n"
      << "tls_base_sync_primitives_disallowed "
      << tls_base_sync_primitives_disallowed;
}

ScopedAllowUnresponsiveTasksForTesting::ScopedAllowUnresponsiveTasksForTesting()
    : base_sync_resetter_(&tls_base_sync_primitives_disallowed,
                          BooleanWithOptionalStack(false)),
      blocking_resetter_(&tls_blocking_disallowed,
                         BooleanWithOptionalStack(false)),
      cpu_resetter_(&tls_cpu_intensive_work_disallowed,
                    BooleanWithOptionalStack(false)) {}

ScopedAllowUnresponsiveTasksForTesting::
    ~ScopedAllowUnresponsiveTasksForTesting() {
  DCHECK(!tls_base_sync_primitives_disallowed)
      << "~ScopedAllowUnresponsiveTasksForTesting() running while "  // IN-TEST
         "surprisingly already no longer allowed.\n"
      << "tls_base_sync_primitives_disallowed "
      << tls_base_sync_primitives_disallowed;
  DCHECK(!tls_blocking_disallowed)
      << "~ScopedAllowUnresponsiveTasksForTesting() running while "  // IN-TEST
         "surprisingly already no longer allowed.\n"
      << "tls_blocking_disallowed " << tls_blocking_disallowed;
  DCHECK(!tls_cpu_intensive_work_disallowed)
      << "~ScopedAllowUnresponsiveTasksForTesting() running while "  // IN-TEST
         "surprisingly already no longer allowed.\n"
      << "tls_cpu_intensive_work_disallowed "
      << tls_cpu_intensive_work_disallowed;
}

namespace internal {

void AssertBaseSyncPrimitivesAllowed() {
  DUMP_OR_DCHECK(!tls_base_sync_primitives_disallowed)
      << "Waiting on a //base sync primitive is not allowed on this thread to "
         "prevent jank and deadlock. If waiting on a //base sync primitive is "
         "unavoidable, do it within the scope of a "
         "ScopedAllowBaseSyncPrimitives. If in a test, use "
         "ScopedAllowBaseSyncPrimitivesForTesting.\n"
      << "tls_base_sync_primitives_disallowed "
      << tls_base_sync_primitives_disallowed
      << "It can be useful to know that tls_blocking_disallowed is "
      << tls_blocking_disallowed;
}

void ResetThreadRestrictionsForTesting() {
  tls_blocking_disallowed = BooleanWithOptionalStack(false);
  tls_singleton_disallowed = BooleanWithOptionalStack(false);
  tls_base_sync_primitives_disallowed = BooleanWithOptionalStack(false);
  tls_cpu_intensive_work_disallowed = BooleanWithOptionalStack(false);
}

void AssertSingletonAllowed() {
  DUMP_OR_DCHECK(!tls_singleton_disallowed)
      << "LazyInstance/Singleton is not allowed to be used on this thread. "
         "Most likely it's because this thread is not joinable (or the current "
         "task is running with TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN "
         "semantics), so AtExitManager may have deleted the object on "
         "shutdown, leading to a potential shutdown crash. If you need to use "
         "the object from this context, it'll have to be updated to use Leaky "
         "traits.\n"
      << "tls_singleton_disallowed " << tls_singleton_disallowed;
}

}  // namespace internal

void DisallowSingleton() {
  tls_singleton_disallowed = BooleanWithOptionalStack(true);
}

ScopedDisallowSingleton::ScopedDisallowSingleton()
    : resetter_(&tls_singleton_disallowed, BooleanWithOptionalStack(true)) {}

ScopedDisallowSingleton::~ScopedDisallowSingleton() {
  DCHECK(tls_singleton_disallowed)
      << "~ScopedDisallowSingleton() running while surprisingly already no "
         "longer disallowed.\n"
      << "tls_singleton_disallowed " << tls_singleton_disallowed;
}

void AssertLongCPUWorkAllowed() {
  DUMP_OR_DCHECK(!tls_cpu_intensive_work_disallowed)
      << "Function marked as CPU intensive was called from a scope that "
         "disallows this kind of work! Consider making this work "
         "asynchronous.\n"
      << "tls_cpu_intensive_work_disallowed "
      << tls_cpu_intensive_work_disallowed;
}

void DisallowUnresponsiveTasks() {
  DisallowBlocking();
  DisallowBaseSyncPrimitives();
  tls_cpu_intensive_work_disallowed = BooleanWithOptionalStack(true);
}

// static
void PermanentThreadAllowance::AllowBlocking() {
  tls_blocking_disallowed = BooleanWithOptionalStack(false);
}

// static
void PermanentThreadAllowance::AllowBaseSyncPrimitives() {
  tls_base_sync_primitives_disallowed = BooleanWithOptionalStack(false);
}

ScopedAllowBlocking::ScopedAllowBlocking(const Location& from_here)
    : resetter_(&tls_blocking_disallowed, BooleanWithOptionalStack(false)) {
  TRACE_EVENT_BEGIN(
      "base", "ScopedAllowBlocking", [&](perfetto::EventContext ctx) {
        ctx.event()->set_source_location_iid(
            base::trace_event::InternedSourceLocation::Get(&ctx, from_here));
      });
}

ScopedAllowBlocking::~ScopedAllowBlocking() {
  TRACE_EVENT_END0("base", "ScopedAllowBlocking");

  DCHECK(!tls_blocking_disallowed)
      << "~ScopedAllowBlocking() running while surprisingly already no longer "
         "allowed.\n"
      << "tls_blocking_disallowed " << tls_blocking_disallowed;
}

ScopedAllowBaseSyncPrimitivesOutsideBlockingScope::
    ScopedAllowBaseSyncPrimitivesOutsideBlockingScope(const Location& from_here)
    : resetter_(&tls_base_sync_primitives_disallowed,
                BooleanWithOptionalStack(false)) {
  TRACE_EVENT_BEGIN(
      "base", "ScopedAllowBaseSyncPrimitivesOutsideBlockingScope",
      [&](perfetto::EventContext ctx) {
        ctx.event()->set_source_location_iid(
            base::trace_event::InternedSourceLocation::Get(&ctx, from_here));
      });

  // Since this object is used to indicate that sync primitives will be used to
  // wait for an event ignore the current operation for hang watching purposes
  // since the wait time duration is unknown.
  base::HangWatcher::InvalidateActiveExpectations();
}

ScopedAllowBaseSyncPrimitivesOutsideBlockingScope::
    ~ScopedAllowBaseSyncPrimitivesOutsideBlockingScope() {
  TRACE_EVENT_END0("base", "ScopedAllowBaseSyncPrimitivesOutsideBlockingScope");

  DCHECK(!tls_base_sync_primitives_disallowed)
      << "~ScopedAllowBaseSyncPrimitivesOutsideBlockingScope() running while "
         "surprisingly already no longer allowed.\n"
      << "tls_base_sync_primitives_disallowed "
      << tls_base_sync_primitives_disallowed;
}

}  // namespace base
