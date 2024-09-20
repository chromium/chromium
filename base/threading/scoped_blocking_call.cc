// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/scoped_blocking_call.h"

#include "base/lazy_instance.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/trace_event/base_tracing.h"
#include "base/tracing_buildflags.h"
#include "build/build_config.h"

#if BUILDFLAG(ENABLE_BASE_TRACING)
#include "third_party/perfetto/protos/perfetto/trace/track_event/source_location.pbzero.h"  // nogncheck
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)

#if DCHECK_IS_ON()
#include "base/auto_reset.h"
#endif

namespace base {

namespace {

#if DCHECK_IS_ON()
// Used to verify that the trace events used in the constructor do not result in
// instantiating a ScopedBlockingCall themselves (which would cause an infinite
// reentrancy loop).
constinit thread_local bool construction_in_progress = false;
#endif

}  // namespace

ScopedBlockingCall::ScopedBlockingCall(const Location& from_here,
                                       BlockingType blocking_type)
    : UncheckedScopedBlockingCall(
          blocking_type,
          UncheckedScopedBlockingCall::BlockingCallType::kRegular) {
#if DCHECK_IS_ON()
  const AutoReset<bool> resetter(&construction_in_progress, true, false);
#endif

  internal::AssertBlockingAllowed();
  TRACE_EVENT_BEGIN(
      "base", "ScopedBlockingCall", [&](perfetto::EventContext ctx) {
        ctx.event()->set_source_location_iid(
            base::trace_event::InternedSourceLocation::Get(&ctx, from_here));
      });
}

ScopedBlockingCall::~ScopedBlockingCall() {
  TRACE_EVENT_END("base");
}

namespace internal {

ScopedBlockingCallWithBaseSyncPrimitives::
    ScopedBlockingCallWithBaseSyncPrimitives(const Location& from_here,
                                             BlockingType blocking_type)
    : UncheckedScopedBlockingCall(
          blocking_type,
          UncheckedScopedBlockingCall::BlockingCallType::kBaseSyncPrimitives) {
#if DCHECK_IS_ON()
  const AutoReset<bool> resetter(&construction_in_progress, true, false);
#endif

  internal::AssertBaseSyncPrimitivesAllowed();
  TRACE_EVENT_BEGIN(
      "base", "ScopedBlockingCallWithBaseSyncPrimitives",
      [&](perfetto::EventContext ctx) {
        perfetto::protos::pbzero::SourceLocation* source_location_data =
            ctx.event()->set_source_location();
        source_location_data->set_file_name(from_here.file_name());
        source_location_data->set_function_name(from_here.function_name());
      });
}

ScopedBlockingCallWithBaseSyncPrimitives::
    ~ScopedBlockingCallWithBaseSyncPrimitives() {
  TRACE_EVENT_END("base");
}

}  // namespace internal

}  // namespace base
