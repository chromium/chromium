// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/thread_restrictions.h"

#include "base/threading/hang_watcher.h"
#include "base/trace_event/base_tracing.h"
#include "build/build_config.h"

#if DCHECK_IS_ON()
#include <utility>

#include "base/check_op.h"
#include "base/debug/stack_trace.h"
#include "base/no_destructor.h"
#include "base/threading/thread_local.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

class BooleanWithStack {
 public:
  // Default value.
  BooleanWithStack() = default;

  // Value when explicitly set.
  explicit BooleanWithStack(bool value) : value_(value) {
    if (kCaptureStackTraces)
      stack_.emplace();
  }

  BooleanWithStack(const BooleanWithStack&) = delete;
  BooleanWithStack& operator=(const BooleanWithStack&) = delete;

  explicit operator bool() const { return value_; }

  friend std::ostream& operator<<(std::ostream& out,
                                  const BooleanWithStack& bws) {
    out << bws.value_;
    if (kCaptureStackTraces) {
      if (bws.stack_.has_value())
        out << " set by\n" << bws.stack_.value();
      else
        out << " (value by default)";
    }
    return out;
  }

 private:
  const bool value_ = false;
  absl::optional<debug::StackTrace> stack_;
};

namespace {

ThreadLocalOwnedPointer<BooleanWithStack>& GetBlockingDisallowedTls() {
  static NoDestructor<ThreadLocalOwnedPointer<BooleanWithStack>> instance;
  auto& tls = *instance;
  if (!tls.Get())
    tls.Set(std::make_unique<BooleanWithStack>());
  return tls;
}
ThreadLocalOwnedPointer<BooleanWithStack>& GetSingletonDisallowedTls() {
  static NoDestructor<ThreadLocalOwnedPointer<BooleanWithStack>> instance;
  auto& tls = *instance;
  if (!tls.Get())
    tls.Set(std::make_unique<BooleanWithStack>());
  return tls;
}
ThreadLocalOwnedPointer<BooleanWithStack>&
GetBaseSyncPrimitivesDisallowedTls() {
  static NoDestructor<ThreadLocalOwnedPointer<BooleanWithStack>> instance;
  auto& tls = *instance;
  if (!tls.Get())
    tls.Set(std::make_unique<BooleanWithStack>());
  return tls;
}
ThreadLocalOwnedPointer<BooleanWithStack>& GetCPUIntensiveWorkDisallowedTls() {
  static NoDestructor<ThreadLocalOwnedPointer<BooleanWithStack>> instance;
  auto& tls = *instance;
  if (!tls.Get())
    tls.Set(std::make_unique<BooleanWithStack>());
  return tls;
}

}  // namespace

namespace internal {

void AssertBlockingAllowed() {
  DCHECK(!*GetBlockingDisallowedTls())
      << "Function marked as blocking was called from a scope that disallows "
         "blocking! If this task is running inside the ThreadPool, it needs "
         "to have MayBlock() in its TaskTraits. Otherwise, consider making "
         "this blocking work asynchronous or, as a last resort, you may use "
         "ScopedAllowBlocking (see its documentation for best practices).\n"
      << "g_blocking_disallowed " << *GetBlockingDisallowedTls();
}

void AssertBlockingDisallowedForTesting() {
  DCHECK(*GetBlockingDisallowedTls())
      << "g_blocking_disallowed " << *GetBlockingDisallowedTls();
}

}  // namespace internal

void DisallowBlocking() {
  GetBlockingDisallowedTls().Set(std::make_unique<BooleanWithStack>(true));
}

ScopedDisallowBlocking::ScopedDisallowBlocking()
    : was_disallowed_(GetBlockingDisallowedTls().Set(
          std::make_unique<BooleanWithStack>(true))) {}

ScopedDisallowBlocking::~ScopedDisallowBlocking() {
  DCHECK(*GetBlockingDisallowedTls())
      << "~ScopedDisallowBlocking() running while surprisingly already no "
         "longer disallowed.\n"
      << "g_blocking_disallowed " << *GetBlockingDisallowedTls();
  GetBlockingDisallowedTls().Set(std::move(was_disallowed_));
}

void DisallowBaseSyncPrimitives() {
  GetBaseSyncPrimitivesDisallowedTls().Set(
      std::make_unique<BooleanWithStack>(true));
}

ScopedDisallowBaseSyncPrimitives::ScopedDisallowBaseSyncPrimitives()
    : was_disallowed_(GetBaseSyncPrimitivesDisallowedTls().Set(
          std::make_unique<BooleanWithStack>(true))) {}

ScopedDisallowBaseSyncPrimitives::~ScopedDisallowBaseSyncPrimitives() {
  DCHECK(*GetBaseSyncPrimitivesDisallowedTls())
      << "~ScopedDisallowBaseSyncPrimitives() running while surprisingly "
         "already no longer disallowed.\n"
      << "g_base_sync_primitives_disallowed "
      << *GetBaseSyncPrimitivesDisallowedTls();
  GetBaseSyncPrimitivesDisallowedTls().Set(std::move(was_disallowed_));
}

ScopedAllowBaseSyncPrimitives::ScopedAllowBaseSyncPrimitives()
    : was_disallowed_(GetBaseSyncPrimitivesDisallowedTls().Set(
          std::make_unique<BooleanWithStack>(false))) {
  DCHECK(!*GetBlockingDisallowedTls())
      << "To allow //base sync primitives in a scope where blocking is "
         "disallowed use ScopedAllowBaseSyncPrimitivesOutsideBlockingScope.\n"
      << "g_blocking_disallowed " << *GetBlockingDisallowedTls();
}

ScopedAllowBaseSyncPrimitives::~ScopedAllowBaseSyncPrimitives() {
  DCHECK(!*GetBaseSyncPrimitivesDisallowedTls());
  GetBaseSyncPrimitivesDisallowedTls().Set(std::move(was_disallowed_));
}

ScopedAllowBaseSyncPrimitivesForTesting::
    ScopedAllowBaseSyncPrimitivesForTesting()
    : was_disallowed_(GetBaseSyncPrimitivesDisallowedTls().Set(
          std::make_unique<BooleanWithStack>(false))) {}

ScopedAllowBaseSyncPrimitivesForTesting::
    ~ScopedAllowBaseSyncPrimitivesForTesting() {
  DCHECK(!*GetBaseSyncPrimitivesDisallowedTls());
  GetBaseSyncPrimitivesDisallowedTls().Set(std::move(was_disallowed_));
}

ScopedAllowUnresponsiveTasksForTesting::ScopedAllowUnresponsiveTasksForTesting()
    : was_disallowed_base_sync_(GetBaseSyncPrimitivesDisallowedTls().Set(
          std::make_unique<BooleanWithStack>(false))),
      was_disallowed_blocking_(GetBlockingDisallowedTls().Set(
          std::make_unique<BooleanWithStack>(false))),
      was_disallowed_cpu_(GetCPUIntensiveWorkDisallowedTls().Set(
          std::make_unique<BooleanWithStack>(false))) {}

ScopedAllowUnresponsiveTasksForTesting::
    ~ScopedAllowUnresponsiveTasksForTesting() {
  DCHECK(!*GetBaseSyncPrimitivesDisallowedTls());
  DCHECK(!*GetBlockingDisallowedTls());
  DCHECK(!*GetCPUIntensiveWorkDisallowedTls());
  GetBaseSyncPrimitivesDisallowedTls().Set(
      std::move(was_disallowed_base_sync_));
  GetBlockingDisallowedTls().Set(std::move(was_disallowed_blocking_));
  GetCPUIntensiveWorkDisallowedTls().Set(std::move(was_disallowed_cpu_));
}

namespace internal {

void AssertBaseSyncPrimitivesAllowed() {
  DCHECK(!*GetBaseSyncPrimitivesDisallowedTls())
      << "Waiting on a //base sync primitive is not allowed on this thread to "
         "prevent jank and deadlock. If waiting on a //base sync primitive is "
         "unavoidable, do it within the scope of a "
         "ScopedAllowBaseSyncPrimitives. If in a test, "
         "use ScopedAllowBaseSyncPrimitivesForTesting.\n"
      << "g_base_sync_primitives_disallowed "
      << *GetBaseSyncPrimitivesDisallowedTls()
      << "It can be useful to know that g_blocking_disallowed is "
      << *GetBlockingDisallowedTls();
}

void ResetThreadRestrictionsForTesting() {
  GetBlockingDisallowedTls().Set(std::make_unique<BooleanWithStack>(false));
  GetSingletonDisallowedTls().Set(std::make_unique<BooleanWithStack>(false));
  GetBaseSyncPrimitivesDisallowedTls().Set(
      std::make_unique<BooleanWithStack>(false));
  GetCPUIntensiveWorkDisallowedTls().Set(
      std::make_unique<BooleanWithStack>(false));
}

void AssertSingletonAllowed() {
  DCHECK(!*GetSingletonDisallowedTls())
      << "LazyInstance/Singleton is not allowed to be used on this thread. "
         "Most likely it's because this thread is not joinable (or the current "
         "task is running with TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN "
         "semantics), so AtExitManager may have deleted the object on "
         "shutdown, leading to a potential shutdown crash. If you need to use "
         "the object from this context, it'll have to be updated to use Leaky "
         "traits.\n"
      << "g_singleton_disallowed " << *GetSingletonDisallowedTls();
}

}  // namespace internal

void DisallowSingleton() {
  GetSingletonDisallowedTls().Set(std::make_unique<BooleanWithStack>(true));
}

ScopedDisallowSingleton::ScopedDisallowSingleton()
    : was_disallowed_(GetSingletonDisallowedTls().Set(
          std::make_unique<BooleanWithStack>(true))) {}

ScopedDisallowSingleton::~ScopedDisallowSingleton() {
  DCHECK(*GetSingletonDisallowedTls())
      << "~ScopedDisallowSingleton() running while surprisingly already no "
         "longer disallowed.\n"
      << "g_singleton_disallowed " << *GetSingletonDisallowedTls();
  GetSingletonDisallowedTls().Set(std::move(was_disallowed_));
}

void AssertLongCPUWorkAllowed() {
  DCHECK(!*GetCPUIntensiveWorkDisallowedTls())
      << "Function marked as CPU intensive was called from a scope that "
         "disallows this kind of work! Consider making this work "
         "asynchronous.\n"
      << "g_cpu_intensive_work_disallowed "
      << *GetCPUIntensiveWorkDisallowedTls();
}

void DisallowUnresponsiveTasks() {
  DisallowBlocking();
  DisallowBaseSyncPrimitives();
  GetCPUIntensiveWorkDisallowedTls().Set(
      std::make_unique<BooleanWithStack>(true));
}

// static
void PermanentThreadAllowance::AllowBlocking() {
  GetBlockingDisallowedTls().Set(std::make_unique<BooleanWithStack>(false));
}

// static
void PermanentThreadAllowance::AllowBaseSyncPrimitives() {
  GetBaseSyncPrimitivesDisallowedTls().Set(
      std::make_unique<BooleanWithStack>(false));
}

// static
void PermanentSingletonAllowance::AllowSingleton() {
  GetSingletonDisallowedTls().Set(std::make_unique<BooleanWithStack>(false));
}

}  // namespace base

#endif  // DCHECK_IS_ON()

namespace base {

ScopedAllowBlocking::ScopedAllowBlocking(const Location& from_here)
#if DCHECK_IS_ON()
    : was_disallowed_(GetBlockingDisallowedTls().Set(
          std::make_unique<BooleanWithStack>(false)))
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
  DCHECK(!*GetBlockingDisallowedTls());
  GetBlockingDisallowedTls().Set(std::move(was_disallowed_));
#endif
}

ScopedAllowBaseSyncPrimitivesOutsideBlockingScope::
    ScopedAllowBaseSyncPrimitivesOutsideBlockingScope(const Location& from_here)
#if DCHECK_IS_ON()
    : was_disallowed_(GetBaseSyncPrimitivesDisallowedTls().Set(
          std::make_unique<BooleanWithStack>(false)))
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
  DCHECK(!*GetBaseSyncPrimitivesDisallowedTls());
  GetBaseSyncPrimitivesDisallowedTls().Set(std::move(was_disallowed_));
#endif
}

}  // namespace base
