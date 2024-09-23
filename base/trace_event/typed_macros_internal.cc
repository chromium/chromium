// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/typed_macros_internal.h"

#include "base/notreached.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros.h"

namespace {

base::ThreadTicks ThreadNow() {
  return base::ThreadTicks::IsSupported()
             ? base::subtle::ThreadTicksNowIgnoringOverride()
             : base::ThreadTicks();
}

base::trace_event::PrepareTrackEventFunction g_typed_event_callback = nullptr;
base::trace_event::PrepareTracePacketFunction g_trace_packet_callback = nullptr;
base::trace_event::EmitEmptyTracePacketFunction g_empty_packet_callback =
    nullptr;

std::pair<char /*phase*/, unsigned long long /*id*/>
GetPhaseAndIdForTraceLog(bool explicit_track, uint64_t track_uuid, char phase) {
  if (!explicit_track)
    return std::make_pair(phase, trace_event_internal::kNoId);

  switch (phase) {
    case TRACE_EVENT_PHASE_BEGIN:
      phase = TRACE_EVENT_PHASE_NESTABLE_ASYNC_BEGIN;
      break;
    case TRACE_EVENT_PHASE_END:
      phase = TRACE_EVENT_PHASE_NESTABLE_ASYNC_END;
      break;
    case TRACE_EVENT_PHASE_INSTANT:
      phase = TRACE_EVENT_PHASE_NESTABLE_ASYNC_INSTANT;
      break;
    default:
      NOTREACHED();
  }
  return std::make_pair(phase, static_cast<unsigned long long>(track_uuid));
}

}  // namespace

namespace trace_event_internal {
const perfetto::Track kDefaultTrack{};
}  // namespace trace_event_internal

namespace base {
namespace trace_event {

void EnableTypedTraceEvents(
    PrepareTrackEventFunction typed_event_callback,
    PrepareTracePacketFunction trace_packet_callback,
    EmitEmptyTracePacketFunction empty_packet_callback) {
  g_typed_event_callback = typed_event_callback;
  g_trace_packet_callback = trace_packet_callback;
  g_empty_packet_callback = empty_packet_callback;
}

void ResetTypedTraceEventsForTesting() {
  g_typed_event_callback = nullptr;
  g_trace_packet_callback = nullptr;
  g_empty_packet_callback = nullptr;
}

TrackEventHandle::TrackEventHandle(TrackEvent* event,
                                   IncrementalState* incremental_state,
                                   CompletionListener* listener,
                                   bool filter_debug_annotations)
    : event_(event),
      incremental_state_(incremental_state),
      listener_(listener),
      filter_debug_annotations_(filter_debug_annotations) {}

TrackEventHandle::TrackEventHandle()
    : TrackEventHandle(nullptr, nullptr, nullptr, false) {}

TrackEventHandle::~TrackEventHandle() {
  if (listener_)
    listener_->OnTrackEventCompleted();
}

TrackEventHandle::CompletionListener::~CompletionListener() = default;

TracePacketHandle::TracePacketHandle(PerfettoPacketHandle packet,
                                     CompletionListener* listener)
    : packet_(std::move(packet)), listener_(listener) {}

TracePacketHandle::TracePacketHandle()
    : TracePacketHandle(PerfettoPacketHandle(), nullptr) {}

TracePacketHandle::~TracePacketHandle() {
  if (listener_)
    listener_->OnTracePacketCompleted();
}

TracePacketHandle::TracePacketHandle(TracePacketHandle&& handle) noexcept {
  *this = std::move(handle);
}

TracePacketHandle& TracePacketHandle::operator=(TracePacketHandle&& handle) {
  this->packet_ = std::move(handle.packet_);
  this->listener_ = handle.listener_;
  return *this;
}

TracePacketHandle::CompletionListener::~CompletionListener() = default;

}  // namespace trace_event
}  // namespace base

namespace trace_event_internal {

base::trace_event::TrackEventHandle CreateTrackEvent(
    char phase,
    const unsigned char* category_group_enabled,
    perfetto::StaticString name,
    base::TimeTicks ts,
    uint64_t track_uuid,
    bool explicit_track) {
  DCHECK(phase == TRACE_EVENT_PHASE_BEGIN || phase == TRACE_EVENT_PHASE_END ||
         phase == TRACE_EVENT_PHASE_INSTANT);
  DCHECK(category_group_enabled);

  if (!g_typed_event_callback)
    return base::trace_event::TrackEventHandle();

  const auto thread_id = base::PlatformThread::CurrentId();
  auto* trace_log = base::trace_event::TraceLog::GetInstance();
  DCHECK(trace_log);

  // Provide events emitted onto different tracks as NESTABLE_ASYNC events to
  // TraceLog, so that e.g. ETW export is aware of them not being a sync event
  // for the current thread.
  auto phase_and_id_for_trace_log =
      GetPhaseAndIdForTraceLog(explicit_track, track_uuid, phase);

  unsigned int flags = TRACE_EVENT_FLAG_NONE;
  if (ts.is_null()) {
    ts = TRACE_TIME_TICKS_NOW();
  } else {
    flags |= TRACE_EVENT_FLAG_EXPLICIT_TIMESTAMP;
  }

  if (!trace_log->ShouldAddAfterUpdatingState(
          phase_and_id_for_trace_log.first, category_group_enabled, name.value,
          phase_and_id_for_trace_log.second, thread_id, ts, nullptr)) {
    return base::trace_event::TrackEventHandle();
  }

  if (phase == TRACE_EVENT_PHASE_INSTANT && !explicit_track) {
    flags |= TRACE_EVENT_SCOPE_THREAD;
  }

  // Only emit thread time / instruction count for events on the default track
  // without explicit timestamp.
  base::ThreadTicks thread_now;
  if ((flags & TRACE_EVENT_FLAG_EXPLICIT_TIMESTAMP) == 0 && !explicit_track) {
    thread_now = ThreadNow();
  }

  base::trace_event::TraceEvent event(
      thread_id, ts, thread_now, phase, category_group_enabled, name.value,
      trace_event_internal::kGlobalScope, trace_event_internal::kNoId,
      trace_event_internal::kNoId, nullptr, flags);

  return g_typed_event_callback(&event);
}

base::trace_event::TracePacketHandle CreateTracePacket() {
  // We only call CreateTracePacket() if the embedder installed a valid
  // g_typed_event_callback, and in that case we also expect a valid
  // g_trace_packet_callback.
  DCHECK(g_trace_packet_callback);
  return g_trace_packet_callback();
}

void AddEmptyPacket() {
  if (g_empty_packet_callback)
    g_empty_packet_callback();
}

bool ShouldEmitTrackDescriptor(
    uint64_t track_uuid,
    base::trace_event::TrackEventHandle::IncrementalState* incr_state) {
  auto it_and_inserted = incr_state->seen_tracks.insert(track_uuid);
  return it_and_inserted.second;
}

}  // namespace trace_event_internal
