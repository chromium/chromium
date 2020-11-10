// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_TYPED_MACROS_INTERNAL_H_
#define BASE_TRACE_EVENT_TYPED_MACROS_INTERNAL_H_

#include "base/base_export.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros_embedder_support.h"
#include "build/build_config.h"
#include "third_party/perfetto/include/perfetto/protozero/message_handle.h"
#include "third_party/perfetto/include/perfetto/tracing/event_context.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/track_event.pbzero.h"

// These macros should not be called directly. They are intended to be used by
// macros in //base/trace_event/typed_macros.h only.

#define TRACING_INTERNAL_CONCAT2(a, b) a##b
#define TRACING_INTERNAL_CONCAT(a, b) TRACING_INTERNAL_CONCAT2(a, b)
#define TRACING_INTERNAL_UID(prefix) TRACING_INTERNAL_CONCAT(prefix, __LINE__)

#define TRACING_INTERNAL_ADD_TRACE_EVENT(phase, category, name, flags, ...) \
  do {                                                                      \
    INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category);                       \
    if (INTERNAL_TRACE_EVENT_CATEGORY_GROUP_ENABLED()) {                    \
      trace_event_internal::AddTraceEvent(                                  \
          phase, INTERNAL_TRACE_EVENT_UID(category_group_enabled), name,    \
          flags, ##__VA_ARGS__);                                            \
    }                                                                       \
  } while (false)

#define TRACING_INTERNAL_SCOPED_ADD_TRACE_EVENT(category, name, ...)          \
  struct {                                                                    \
    struct ScopedTraceEvent {                                                 \
      /* The parameter is an implementation detail. It allows the         */  \
      /* anonymous struct to use aggregate initialization to invoke the   */  \
      /* lambda to emit the begin event with the proper reference capture */  \
      /* for any TrackEventArgumentFunction in |__VA_ARGS__|. This is     */  \
      /* required so that the scoped event is exactly ONE line and can't  */  \
      /* escape the scope if used in a single line if statement.          */  \
      ScopedTraceEvent(...) {}                                                \
      ~ScopedTraceEvent() {                                                   \
        /* TODO(nuskos): Remove the empty string passed as the |name|  */     \
        /* field. As described in macros.h we shouldn't need it in our */     \
        /* end state.                                                  */     \
        TRACING_INTERNAL_ADD_TRACE_EVENT(TRACE_EVENT_PHASE_END, category, "", \
                                         TRACE_EVENT_FLAG_NONE,               \
                                         [](perfetto::EventContext) {});      \
      }                                                                       \
    } event;                                                                  \
  } TRACING_INTERNAL_UID(scoped_event){[&]() {                                \
    TRACING_INTERNAL_ADD_TRACE_EVENT(TRACE_EVENT_PHASE_BEGIN, category, name, \
                                     TRACE_EVENT_FLAG_NONE, ##__VA_ARGS__);   \
    return 0;                                                                 \
  }()};

namespace trace_event_internal {

// Copy of function with the same name from Perfetto client library.
template <typename T>
constexpr bool IsValidTraceLambdaImpl(
    typename std::enable_if<static_cast<bool>(
        sizeof(std::declval<T>()(std::declval<perfetto::EventContext>()),
               0))>::type* = nullptr) {
  return true;
}

template <typename T>
constexpr bool IsValidTraceLambdaImpl(...) {
  return false;
}

template <typename T>
constexpr bool IsValidTraceLambda() {
  return IsValidTraceLambdaImpl<T>(nullptr);
}

// The perfetto client library does not use event names for
// TRACE_EVENT_PHASE_END. However, TraceLog expects all TraceEvents to have
// event names. So, until we move over to the client library, we will use this
// empty name for all TRACE_EVENT_PHASE_END typed events.
constexpr char kTraceEventEndName[] = "";

base::trace_event::TrackEventHandle BASE_EXPORT
CreateTrackEvent(char phase,
                 const unsigned char* category_group_enabled,
                 const char* name,
                 unsigned int flags,
                 base::TimeTicks timestamp,
                 bool explicit_track);

base::trace_event::TracePacketHandle BASE_EXPORT CreateTracePacket();

bool BASE_EXPORT ShouldEmitTrackDescriptor(
    uint64_t track_uuid,
    base::trace_event::TrackEventHandle::IncrementalState* incr_state);

template <typename TrackType>
void WriteTrackDescriptor(const TrackType& track) {
  base::trace_event::TracePacketHandle packet = CreateTracePacket();
  if (!packet)
    return;
  perfetto::internal::TrackRegistry::Get()->SerializeTrack(
      track, packet.TakePerfettoHandle());
}

template <
    typename TrackEventArgumentFunction = void (*)(perfetto::EventContext),
    typename ArgumentFunctionCheck = typename std::enable_if<
        IsValidTraceLambda<TrackEventArgumentFunction>()>::type>
inline void AddTraceEvent(char phase,
                          const unsigned char* category_group_enabled,
                          const char* name,
                          unsigned int flags,
                          const perfetto::Track& track,
                          base::TimeTicks timestamp,
                          TrackEventArgumentFunction argument_func) {
  bool emit_track_descriptor = false;
  {
    base::trace_event::TrackEventHandle track_event =
        CreateTrackEvent(phase, category_group_enabled, name, flags, timestamp,
                         track.uuid != perfetto::Track().uuid);
    if (!track_event)
      return;

    if (track) {
      track_event->set_track_uuid(track.uuid);
      emit_track_descriptor = ShouldEmitTrackDescriptor(
          track.uuid, track_event.incremental_state());
    }

    argument_func(perfetto::EventContext(track_event.get(),
                                         track_event.incremental_state()));
  }

  if (emit_track_descriptor)
    WriteTrackDescriptor(track);
}

template <
    typename TrackEventArgumentFunction = void (*)(perfetto::EventContext),
    typename ArgumentFunctionCheck = typename std::enable_if<
        IsValidTraceLambda<TrackEventArgumentFunction>()>::type,
    typename TrackType,
    typename TrackTypeCheck = typename std::enable_if<
        std::is_convertible<TrackType, perfetto::Track>::value>::type>
inline void AddTraceEvent(char phase,
                          const unsigned char* category_group_enabled,
                          const char* name,
                          unsigned int flags,
                          const TrackType& track,
                          TrackEventArgumentFunction argument_func) {
  AddTraceEvent(phase, category_group_enabled, name, flags, track,
                base::TimeTicks(), argument_func);
}

template <
    typename TrackEventArgumentFunction = void (*)(perfetto::EventContext),
    typename ArgumentFunctionCheck = typename std::enable_if<
        IsValidTraceLambda<TrackEventArgumentFunction>()>::type>
inline void AddTraceEvent(char phase,
                          const unsigned char* category_group_enabled,
                          const char* name,
                          unsigned int flags,
                          TrackEventArgumentFunction argument_func) {
  AddTraceEvent(phase, category_group_enabled, name, flags, perfetto::Track(),
                base::TimeTicks(), argument_func);
}

inline void AddTraceEvent(char phase,
                          const unsigned char* category_group_enabled,
                          const char* name,
                          unsigned int flags) {
  AddTraceEvent(phase, category_group_enabled, name, flags, perfetto::Track(),
                base::TimeTicks(), [](perfetto::EventContext ctx) {});
}

template <typename TrackType,
          typename TrackTypeCheck = typename std::enable_if<
              std::is_convertible<TrackType, perfetto::Track>::value>::type>
inline void AddTraceEvent(char phase,
                          const unsigned char* category_group_enabled,
                          const char* name,
                          unsigned int flags,
                          const TrackType& track) {
  AddTraceEvent(phase, category_group_enabled, name, flags, track,
                base::TimeTicks(), [](perfetto::EventContext ctx) {});
}

template <typename TrackType,
          typename TrackTypeCheck = typename std::enable_if<
              std::is_convertible<TrackType, perfetto::Track>::value>::type>
inline void AddTraceEvent(char phase,
                          const unsigned char* category_group_enabled,
                          const char* name,
                          unsigned int flags,
                          const TrackType& track,
                          base::TimeTicks timestamp) {
  AddTraceEvent(phase, category_group_enabled, name, flags, track, timestamp,
                [](perfetto::EventContext ctx) {});
}

}  // namespace trace_event_internal

#endif  // BASE_TRACE_EVENT_TYPED_MACROS_INTERNAL_H_
