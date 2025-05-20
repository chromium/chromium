// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_COMMON_TRACE_EVENT_COMMON_H_
#define BASE_TRACE_EVENT_COMMON_TRACE_EVENT_COMMON_H_

// See third_party/perfetto/include/perfetto/tracing/track_event.h for
// documentation of Trace Event Macros.

////////////////////////////////////////////////////////////////////////////////
// Perfetto trace macros

#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"

#if BUILDFLAG(ENABLE_BASE_TRACING)

// Enable legacy trace event macros (e.g., TRACE_EVENT{0,1,2}).
#define PERFETTO_ENABLE_LEGACY_TRACE_EVENTS 1

// Macros for reading the current trace time (bypassing any virtual time
// overrides).
#define TRACE_TIME_TICKS_NOW() ::base::subtle::TimeTicksNowIgnoringOverride()
#define TRACE_TIME_NOW() ::base::subtle::TimeNowIgnoringOverride()

// Implementation detail: trace event macros create temporary variables
// to keep instrumentation overhead low. These macros give each temporary
// variable a unique name based on the line number to prevent name collisions.
#define INTERNAL_TRACE_EVENT_UID(name_prefix) PERFETTO_UID(name_prefix)

// Declare debug annotation converters for base time types, so they can be
// passed as trace event arguments.
// TODO(skyostil): Serialize timestamps using perfetto::TracedValue instead.
namespace perfetto {
namespace protos {
namespace pbzero {
class DebugAnnotation;
}  // namespace pbzero
}  // namespace protos
namespace internal {

void BASE_EXPORT
WriteDebugAnnotation(protos::pbzero::DebugAnnotation* annotation,
                     ::base::TimeTicks);
void BASE_EXPORT
WriteDebugAnnotation(protos::pbzero::DebugAnnotation* annotation, ::base::Time);

}  // namespace internal
}  // namespace perfetto

// Pull in the tracing macro definitions from Perfetto.
#include "third_party/perfetto/include/perfetto/tracing/track_event.h"  // IWYU pragma: export
#include "third_party/perfetto/include/perfetto/tracing/track_event_legacy.h"  // IWYU pragma: export

namespace perfetto {
namespace legacy {

template <>
perfetto::ThreadTrack BASE_EXPORT
ConvertThreadId(const ::base::PlatformThreadId& thread);

#if BUILDFLAG(IS_WIN)
template <>
perfetto::ThreadTrack BASE_EXPORT ConvertThreadId(const int& thread);
#endif  // BUILDFLAG(IS_WIN)

}  // namespace legacy

template <>
struct BASE_EXPORT TraceTimestampTraits<::base::TimeTicks> {
  static TraceTimestamp ConvertTimestampToTraceTimeNs(
      const ::base::TimeTicks& ticks);
};

}  // namespace perfetto

#else  // !BUILDFLAG(ENABLE_BASE_TRACING)

// This macro is still used in some components even when base tracing is
// disabled.
// TODO(crbug/336718643): Make sure no code affected by
// enable_base_tracing=false includes this file directly, then move the define
// to trace_event_stub.h.
#define TRACE_DISABLED_BY_DEFAULT(name) "disabled-by-default-" name

#endif  // !BUILDFLAG(ENABLE_BASE_TRACING)

#endif  // BASE_TRACE_EVENT_COMMON_TRACE_EVENT_COMMON_H_
