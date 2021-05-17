// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_TYPED_MACROS_INTERNAL_H_
#define BASE_TRACE_EVENT_TYPED_MACROS_INTERNAL_H_

#include "base/base_export.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros_embedder_support.h"
#include "build/build_config.h"
#include "third_party/perfetto/include/perfetto/protozero/message_handle.h"
#include "third_party/perfetto/include/perfetto/tracing/event_context.h"
#include "third_party/perfetto/include/perfetto/tracing/internal/write_track_event_args.h"
#include "third_party/perfetto/include/perfetto/tracing/string_helpers.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/track_event.pbzero.h"

// These macros should not be called directly. They are intended to be used by
// macros in //base/trace_event/typed_macros.h only.

#define TRACING_INTERNAL_CONCAT2(a, b) a##b
#define TRACING_INTERNAL_CONCAT(a, b) TRACING_INTERNAL_CONCAT2(a, b)
#define TRACING_INTERNAL_UID(prefix) TRACING_INTERNAL_CONCAT(prefix, __LINE__)

#define TRACING_INTERNAL_ADD_TRACE_EVENT(phase, category, name, ...)     \
  do {                                                                   \
    INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category);                    \
    if (INTERNAL_TRACE_EVENT_CATEGORY_GROUP_ENABLED()) {                 \
      trace_event_internal::AddTypedTraceEvent(                          \
          phase, INTERNAL_TRACE_EVENT_UID(category_group_enabled), name, \
          ##__VA_ARGS__);                                                \
    }                                                                    \
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
                                         [](perfetto::EventContext) {});      \
      }                                                                       \
    } event;                                                                  \
  } TRACING_INTERNAL_UID(scoped_event){[&]() {                                \
    TRACING_INTERNAL_ADD_TRACE_EVENT(TRACE_EVENT_PHASE_BEGIN, category, name, \
                                     ##__VA_ARGS__);                          \
    return 0;                                                                 \
  }()};

namespace trace_event_internal {

extern BASE_EXPORT const perfetto::Track kDefaultTrack;

// The perfetto client library does not use event names for
// TRACE_EVENT_PHASE_END. However, TraceLog expects all TraceEvents to have
// event names. So, until we move over to the client library, we will use this
// empty name for all TRACE_EVENT_PHASE_END typed events.
constexpr char kTraceEventEndName[] = "";

base::trace_event::TrackEventHandle BASE_EXPORT
CreateTrackEvent(char phase,
                 const unsigned char* category_group_enabled,
                 perfetto::StaticString name,
                 base::TimeTicks timestamp,
                 uint64_t track_uuid,
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

template <typename... Args>
inline void AddTypedTraceEventImpl(char phase,
                                   const unsigned char* category_group_enabled,
                                   perfetto::StaticString name,
                                   const perfetto::Track& track,
                                   base::TimeTicks timestamp,
                                   Args&&... args) {
  bool emit_track_descriptor = false;
  {
    bool explicit_track = &track != &kDefaultTrack;
    base::trace_event::TrackEventHandle track_event =
        CreateTrackEvent(phase, category_group_enabled, name, timestamp,
                         track.uuid, explicit_track);
    if (!track_event)
      return;

    if (explicit_track) {
      track_event->set_track_uuid(track.uuid);
      emit_track_descriptor = ShouldEmitTrackDescriptor(
          track.uuid, track_event.incremental_state());
    }

    perfetto::internal::WriteTrackEventArgs(
        perfetto::EventContext(track_event.get(),
                               track_event.incremental_state()),
        std::forward<Args>(args)...);
  }

  if (emit_track_descriptor)
    WriteTrackDescriptor(track);
}

template <typename TrackType,
          typename... Args,
          typename TrackTypeCheck = typename std::enable_if<
              std::is_convertible<TrackType, perfetto::Track>::value>::type>
inline void AddTypedTraceEvent(char phase,
                               const unsigned char* category_group_enabled,
                               perfetto::StaticString name,
                               TrackType&& track,
                               base::TimeTicks timestamp,
                               Args&&... args) {
  AddTypedTraceEventImpl(phase, category_group_enabled, name,
                         std::forward<TrackType>(track), timestamp,
                         std::forward<Args>(args)...);
}

template <typename TrackType,
          typename... Args,
          typename TrackTypeCheck = typename std::enable_if<
              std::is_convertible<TrackType, perfetto::Track>::value>::type>
inline void AddTypedTraceEvent(char phase,
                               const unsigned char* category_group_enabled,
                               perfetto::StaticString name,
                               TrackType&& track,
                               Args&&... args) {
  AddTypedTraceEventImpl(phase, category_group_enabled, name,
                         std::forward<TrackType>(track), base::TimeTicks(),
                         std::forward<Args>(args)...);
}

template <typename... Args>
inline void AddTypedTraceEvent(char phase,
                               const unsigned char* category_group_enabled,
                               perfetto::StaticString name,
                               base::TimeTicks timestamp,
                               Args&&... args) {
  AddTypedTraceEventImpl(phase, category_group_enabled, name, kDefaultTrack,
                         timestamp, std::forward<Args>(args)...);
}

template <typename... Args>
inline void AddTypedTraceEvent(char phase,
                               const unsigned char* category_group_enabled,
                               perfetto::StaticString name,
                               Args&&... args) {
  AddTypedTraceEventImpl(phase, category_group_enabled, name, kDefaultTrack,
                         base::TimeTicks(), std::forward<Args>(args)...);
}

}  // namespace trace_event_internal

#endif  // BASE_TRACE_EVENT_TYPED_MACROS_INTERNAL_H_
