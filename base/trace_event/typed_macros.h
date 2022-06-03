// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_TYPED_MACROS_H_
#define BASE_TRACE_EVENT_TYPED_MACROS_H_

#include "base/base_export.h"
#include "base/trace_event/trace_event.h"
#include "base/tracing_buildflags.h"
#include "build/build_config.h"

// Needed not for this file, but for every user of the TRACE_EVENT macros for
// the lambda definition. So included here for convenience.
#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "third_party/perfetto/include/perfetto/tracing/event_context.h"
#include "third_party/perfetto/include/perfetto/tracing/string_helpers.h"

#if !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
#include "base/trace_event/typed_macros_internal.h"

#if defined(TRACE_EVENT_BEGIN)
#error "Another copy of perfetto tracing macros have been included"
#endif

// This file implements typed event macros [1,2] that will be provided by the
// Perfetto client library in the future, as a stop-gap to support typed trace
// events in Chrome until we are ready to switch to the client library's
// implementation of trace macros.
// [1] https://perfetto.dev/docs/instrumentation/track-events
// [2] //third_party/perfetto/include/perfetto/tracing/track_event.h
// TODO(crbug/1006541): Replace this file with the Perfetto client library.

// Typed event macros:
//
// These macros emit a slice under |category| with the title |name|. Both
// strings must be static constants. The track event is only recorded if
// |category| is enabled for the tracing session.
//
// The slice is thread-scoped (i.e., written to the default track of the current
// thread) unless overridden with a custom track object (see perfetto::Track).
//
// |name| must be a string with static lifetime (i.e., the same address must not
// be used for a different event name in the future). If you want to use a
// dynamically allocated name, do this:
//
//   TRACE_EVENT("category", nullptr, [&](perfetto::EventContext ctx) {
//     ctx.event()->set_name(dynamic_name);
//   });
//
// The varargs can include a perfetto::Track (e.g. async events), a
// base::TimeTicks timestamp, and a trace lambda. If passed, the lambda is
// executed synchronously.
//
// Examples:
//
//   // Sync event with typed field.
//   TRACE_EVENT("cat", "Name", [](perfetto::EventContext ctx) {
//       auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
//       // Fill in some field in event.
//       event->set_my_chrome_field();
//   });
//
//   // Async event.
//   TRACE_EVENT_BEGIN("cat", "Name", perfetto::Track(1234));
//
//   // Async event with explicit timestamp.
//   base::TimeTicks time_ticks;
//   TRACE_EVENT_BEGIN("cat", "Name", perfetto::Track(1234), time_ticks);

// Begin a slice under |category| with the title |name|.
// Defaults to the current thread's track.
#define TRACE_EVENT_BEGIN(category, name, ...)                              \
  TRACING_INTERNAL_ADD_TRACE_EVENT(TRACE_EVENT_PHASE_BEGIN, category, name, \
                                   ##__VA_ARGS__)

// End a slice under |category|.
// Defaults to the current thread's track.
#define TRACE_EVENT_END(category, ...)                                       \
  TRACING_INTERNAL_ADD_TRACE_EVENT(TRACE_EVENT_PHASE_END, category,          \
                                   trace_event_internal::kTraceEventEndName, \
                                   ##__VA_ARGS__)

// Begin a thread-scoped slice which gets automatically closed when going out
// of scope.
//
// BEWARE: similarly to TRACE_EVENT_BEGIN, this macro does accept a track, but
// it does not work properly and should not be used.
// TODO(b/154583431): figure out how to fix or disallow that and update the
// comment.
#define TRACE_EVENT(category, name, ...) \
  TRACING_INTERNAL_SCOPED_ADD_TRACE_EVENT(category, name, ##__VA_ARGS__)

// Emit a single slice with title |name| and zero duration.
// Defaults to the current thread's track.
#define TRACE_EVENT_INSTANT(category, name, ...)                              \
  TRACING_INTERNAL_ADD_TRACE_EVENT(TRACE_EVENT_PHASE_INSTANT, category, name, \
                                   ##__VA_ARGS__)

#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
#endif  // BASE_TRACE_EVENT_TYPED_MACROS_H_
