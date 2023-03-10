// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/thread_restrictions.h"

#include "base/threading/hang_watcher.h"
#include "base/trace_event/base_tracing.h"
#include "build/build_config.h"

#if DCHECK_IS_ON()
#include "base/check_op.h"
#include "base/no_destructor.h"
#include "base/threading/thread_local.h"

// NaCL doesn't support stack sampling and Android is slow at stack sampling and
// this causes timeouts (crbug.com/959139).
#if BUILDFLAG(IS_NACL) || BUILDFLAG(IS_ANDROID)
constexpr bool kCaptureStackTraces = false;
#else
// Always disabled when !EXPENSIVE_DCHECKS_ARE_ON() because user-facing builds
// typically drop log strings anyways.
constexpr bool kCaptureStackTraces = EXPENSIVE_DCHECKS_ARE_ON();
#endif

namespace base {

BooleanWithStack::BooleanWithStack(bool value) : value_(value) {
  if (kCaptureStackTraces) {
    stack_.emplace();
  }
}

std::ostream& operator<<(std::ostream& out, const BooleanWithStack& bws) {
  out << bws.value_;
  if (kCaptureStackTraces) {
    if (bws.stack_.has_value()) {
      out << " set by\n" << bws.stack_.value();
    } else {
      out << " (value by default)";
    }
  }
  return out;
}

namespace {

// TODO(crbug.com/1423437): Change these to directly-accessed, namespace-scope
// `thread_local BooleanWithStack`s when doing so doesn't cause crashes.
BooleanWithStack& GetBlockingDisallowedTls() {
  static NoDestructor<ThreadLocalOwnedPointer<BooleanWithStack>> instance;
  auto& tls = *instance;
  if (!tls.Get()) {
    tls.Set(std::make_unique<BooleanWithStack>());
  }
  return *tls;
}
BooleanWithStack& GetSingletonDisallowedTls() {
  static NoDestructor<ThreadLocalOwnedPointer<BooleanWithStack>> instance;
  auto& tls = *instance;
  if (!tls.Get()) {
    tls.Set(std::make_unique<BooleanWithStack>());
  }
  return *tls;
}
BooleanWithStack& GetBaseSyncPrimitivesDisallowedTls() {
  static NoDestructor<ThreadLocalOwnedPointer<BooleanWithStack>> instance;
  auto& tls = *instance;
  if (!tls.Get()) {
    tls.Set(std::make_unique<BooleanWithStack>());
  }
  return *tls;
}
BooleanWithStack& GetCPUIntensiveWorkDisallowedTls() {
  static NoDestructor<ThreadLocalOwnedPointer<BooleanWithStack>> instance;
  auto& tls = *instance;
  if (!tls.Get()) {
    tls.Set(std::make_unique<BooleanWithStack>());
  }
  return *tls;
}

}  // namespace

namespace internal {

void AssertBlockingAllowed() {
  DCHECK(!GetBlockingDisallowedTls())
      << "Function marked as blocking was called from a scope that disallows "
         "blocking! If this task is running inside the ThreadPool, it needs "
         "to have MayBlock() in its TaskTraits. Otherwise, consider making "
         "this blocking work asynchronous or, as a last resort, you may use "
         "ScopedAllowBlocking (see its documentation for best practices).\n"
      << "blocking_disallowed " << GetBlockingDisallowedTls();
}

void AssertBlockingDisallowedForTesting() {
  DCHECK(GetBlockingDisallowedTls())
      << "blocking_disallowed " << GetBlockingDisallowedTls();
}

}  // namespace internal

void DisallowBlocking() {
  GetBlockingDisallowedTls() = BooleanWithStack(true);
}

ScopedDisallowBlocking::ScopedDisallowBlocking()
    : resetter_(&GetBlockingDisallowedTls(), BooleanWithStack(true)) {}

ScopedDisallowBlocking::~ScopedDisallowBlocking() {
  DCHECK(GetBlockingDisallowedTls())
      << "~ScopedDisallowBlocking() running while surprisingly already no "
         "longer disallowed.\n"
      << "blocking_disallowed " << GetBlockingDisallowedTls();
}

void DisallowBaseSyncPrimitives() {
  GetBaseSyncPrimitivesDisallowedTls() = BooleanWithStack(true);
}

ScopedDisallowBaseSyncPrimitives::ScopedDisallowBaseSyncPrimitives()
    : resetter_(&GetBaseSyncPrimitivesDisallowedTls(), BooleanWithStack(true)) {
}

ScopedDisallowBaseSyncPrimitives::~ScopedDisallowBaseSyncPrimitives() {
  DCHECK(GetBaseSyncPrimitivesDisallowedTls())
      << "~ScopedDisallowBaseSyncPrimitives() running while surprisingly "
         "already no longer disallowed.\n"
      << "base_sync_primitives_disallowed "
      << GetBaseSyncPrimitivesDisallowedTls();
}

ScopedAllowBaseSyncPrimitives::ScopedAllowBaseSyncPrimitives()
    : resetter_(&GetBaseSyncPrimitivesDisallowedTls(),
                BooleanWithStack(false)) {
  DCHECK(!GetBlockingDisallowedTls())
      << "To allow //base sync primitives in a scope where blocking is "
         "disallowed use ScopedAllowBaseSyncPrimitivesOutsideBlockingScope.\n"
      << "blocking_disallowed " << GetBlockingDisallowedTls();
}

ScopedAllowBaseSyncPrimitives::~ScopedAllowBaseSyncPrimitives() {
  DCHECK(!GetBaseSyncPrimitivesDisallowedTls())
      << "~ScopedAllowBaseSyncPrimitives() running while surprisingly already "
         "no longer allowed.\n"
      << "base_sync_primitives_disallowed "
      << GetBaseSyncPrimitivesDisallowedTls();
}

ScopedAllowBaseSyncPrimitivesForTesting::
    ScopedAllowBaseSyncPrimitivesForTesting()
    : resetter_(&GetBaseSyncPrimitivesDisallowedTls(),
                BooleanWithStack(false)) {}

ScopedAllowBaseSyncPrimitivesForTesting::
    ~ScopedAllowBaseSyncPrimitivesForTesting() {
  DCHECK(!GetBaseSyncPrimitivesDisallowedTls())
      << "~ScopedAllowBaseSyncPrimitivesForTesting() running while "  // IN-TEST
         "surprisingly already no longer allowed.\n"
      << "base_sync_primitives_disallowed "
      << GetBaseSyncPrimitivesDisallowedTls();
}

ScopedAllowUnresponsiveTasksForTesting::ScopedAllowUnresponsiveTasksForTesting()
    : base_sync_resetter_(&GetBaseSyncPrimitivesDisallowedTls(),
                          BooleanWithStack(false)),
      blocking_resetter_(&GetBlockingDisallowedTls(), BooleanWithStack(false)),
      cpu_resetter_(&GetCPUIntensiveWorkDisallowedTls(),
                    BooleanWithStack(false)) {}

ScopedAllowUnresponsiveTasksForTesting::
    ~ScopedAllowUnresponsiveTasksForTesting() {
  DCHECK(!GetBaseSyncPrimitivesDisallowedTls())
      << "~ScopedAllowUnresponsiveTasksForTesting() running while "  // IN-TEST
         "surprisingly already no longer allowed.\n"
      << "base_sync_primitives_disallowed "
      << GetBaseSyncPrimitivesDisallowedTls();
  DCHECK(!GetBlockingDisallowedTls())
      << "~ScopedAllowUnresponsiveTasksForTesting() running while "  // IN-TEST
         "surprisingly already no longer allowed.\n"
      << "blocking_disallowed " << GetBlockingDisallowedTls();
  DCHECK(!GetCPUIntensiveWorkDisallowedTls())
      << "~ScopedAllowUnresponsiveTasksForTesting() running while "  // IN-TEST
         "surprisingly already no longer allowed.\n"
      << "cpu_intensive_work_disallowed " << GetCPUIntensiveWorkDisallowedTls();
}

namespace internal {

void AssertBaseSyncPrimitivesAllowed() {
  DCHECK(!GetBaseSyncPrimitivesDisallowedTls())
      << "Waiting on a //base sync primitive is not allowed on this thread to "
         "prevent jank and deadlock. If waiting on a //base sync primitive is "
         "unavoidable, do it within the scope of a "
         "ScopedAllowBaseSyncPrimitives. If in a test, use "
         "ScopedAllowBaseSyncPrimitivesForTesting.\n"
      << "base_sync_primitives_disallowed "
      << GetBaseSyncPrimitivesDisallowedTls()
      << "It can be useful to know that blocking_disallowed is "
      << GetBlockingDisallowedTls();
}

void ResetThreadRestrictionsForTesting() {
  GetBlockingDisallowedTls() = BooleanWithStack(false);
  GetSingletonDisallowedTls() = BooleanWithStack(false);
  GetBaseSyncPrimitivesDisallowedTls() = BooleanWithStack(false);
  GetCPUIntensiveWorkDisallowedTls() = BooleanWithStack(false);
}

void AssertSingletonAllowed() {
  DCHECK(!GetSingletonDisallowedTls())
      << "LazyInstance/Singleton is not allowed to be used on this thread. "
         "Most likely it's because this thread is not joinable (or the current "
         "task is running with TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN "
         "semantics), so AtExitManager may have deleted the object on "
         "shutdown, leading to a potential shutdown crash. If you need to use "
         "the object from this context, it'll have to be updated to use Leaky "
         "traits.\n"
      << "singleton_disallowed " << GetSingletonDisallowedTls();
}

}  // namespace internal

void DisallowSingleton() {
  GetSingletonDisallowedTls() = BooleanWithStack(true);
}

ScopedDisallowSingleton::ScopedDisallowSingleton()
    : resetter_(&GetSingletonDisallowedTls(), BooleanWithStack(true)) {}

ScopedDisallowSingleton::~ScopedDisallowSingleton() {
  DCHECK(GetSingletonDisallowedTls())
      << "~ScopedDisallowSingleton() running while surprisingly already no "
         "longer disallowed.\n"
      << "singleton_disallowed " << GetSingletonDisallowedTls();
}

void AssertLongCPUWorkAllowed() {
  DCHECK(!GetCPUIntensiveWorkDisallowedTls())
      << "Function marked as CPU intensive was called from a scope that "
         "disallows this kind of work! Consider making this work "
         "asynchronous.\n"
      << "cpu_intensive_work_disallowed " << GetCPUIntensiveWorkDisallowedTls();
}

void DisallowUnresponsiveTasks() {
  DisallowBlocking();
  DisallowBaseSyncPrimitives();
  GetCPUIntensiveWorkDisallowedTls() = BooleanWithStack(true);
}

// static
void PermanentThreadAllowance::AllowBlocking() {
  GetBlockingDisallowedTls() = BooleanWithStack(false);
}

// static
void PermanentThreadAllowance::AllowBaseSyncPrimitives() {
  GetBaseSyncPrimitivesDisallowedTls() = BooleanWithStack(false);
}

// static
void PermanentSingletonAllowance::AllowSingleton() {
  GetSingletonDisallowedTls() = BooleanWithStack(false);
}

}  // namespace base

#endif  // DCHECK_IS_ON()

namespace base {

ScopedAllowBlocking::ScopedAllowBlocking(const Location& from_here)
#if DCHECK_IS_ON()
    : resetter_(&GetBlockingDisallowedTls(), BooleanWithStack(false))
#endif
{
  TRACE_EVENT_BEGIN(
      "base", "ScopedAllowBlocking", [&](perfetto::EventContext ctx) {
        ctx.event()->set_source_location_iid(
            base::trace_event::InternedSourceLocation::Get(&ctx, from_here));
      });
}

ScopedAllowBlocking::~ScopedAllowBlocking() {
  TRACE_EVENT_END0("base", "ScopedAllowBlocking");

#if DCHECK_IS_ON()
  DCHECK(!GetBlockingDisallowedTls())
      << "~ScopedAllowBlocking() running while surprisingly already no longer "
         "allowed.\n"
      << "blocking_disallowed " << GetBlockingDisallowedTls();
#endif
}

ScopedAllowBaseSyncPrimitivesOutsideBlockingScope::
    ScopedAllowBaseSyncPrimitivesOutsideBlockingScope(const Location& from_here)
#if DCHECK_IS_ON()
    : resetter_(&GetBaseSyncPrimitivesDisallowedTls(), BooleanWithStack(false))
#endif
{
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

#if DCHECK_IS_ON()
  DCHECK(!GetBaseSyncPrimitivesDisallowedTls())
      << "~ScopedAllowBaseSyncPrimitivesOutsideBlockingScope() running while "
         "surprisingly already no longer allowed.\n"
      << "base_sync_primitives_disallowed "
      << GetBaseSyncPrimitivesDisallowedTls();
#endif
}

}  // namespace base
