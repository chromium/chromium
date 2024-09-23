// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_TYPED_MACROS_INTERNAL_H_
#define BASE_TRACE_EVENT_TYPED_MACROS_INTERNAL_H_

#include "base/base_export.h"
#include "base/macros/uniquify.h"
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
// macros in //base/trace_event/typed_macros.h only. With the Perfetto client
// library, these macros are either implemented by Perfetto or unneeded.

namespace trace_event_internal {

extern BASE_EXPORT const perfetto::Track kDefaultTrack;

// The perfetto client library does not use event names for
// TRACE_EVENT_PHASE_END. However, TraceLog expects all TraceEvents to have
// event names. So, until we move over to the client library, we will use this
// empty name for all TRACE_EVENT_PHASE_END typed events.
inline constexpr char kTraceEventEndName[] = "";

base::trace_event::TrackEventHandle BASE_EXPORT
CreateTrackEvent(char phase,
                 const unsigned char* category_group_enabled,
                 perfetto::StaticString name,
                 base::TimeTicks timestamp,
                 uint64_t track_uuid,
                 bool explicit_track);

base::trace_event::TracePacketHandle BASE_EXPORT CreateTracePacket();

void BASE_EXPORT AddEmptyPacket();

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
                               track_event.incremental_state(),
                               track_event.ShouldFilterDebugAnnotations()),
        std::forward<Args>(args)...);
  }

  if (emit_track_descriptor)
    WriteTrackDescriptor(track);
}

template <typename TrackType,
          typename... Args,
          typename TrackTypeCheck = std::enable_if_t<
              std::is_convertible_v<TrackType, perfetto::Track>>>
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
          typename TrackTypeCheck = std::enable_if_t<
              std::is_convertible_v<TrackType, perfetto::Track>>>
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
