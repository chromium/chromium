// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_TYPED_MACROS_EMBEDDER_SUPPORT_H_
#define BASE_TRACE_EVENT_TYPED_MACROS_EMBEDDER_SUPPORT_H_

#include "base/base_export.h"
#include "base/trace_event/trace_event.h"
#include "third_party/perfetto/include/perfetto/tracing/internal/track_event_internal.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/track_event.pbzero.h"

namespace base {
namespace trace_event {

class BASE_EXPORT TrackEventHandle {
 public:
  using TrackEvent = perfetto::protos::pbzero::TrackEvent;
  using IncrementalState = perfetto::internal::TrackEventIncrementalState;

  class BASE_EXPORT CompletionListener {
   public:
    // Implemented in typed_macros_internal.h.
    virtual ~CompletionListener();
    virtual void OnTrackEventCompleted() = 0;
  };

  // Creates a handle to |event| which notifies |listener| on the handle's
  // destruction, i.e. after the event lambda has emitted any typed arguments
  // into the event. Note that |listener| must outlive the TRACE_EVENT call,
  // i.e. cannot be destroyed until OnTrackEventCompleted() is called. Ownership
  // of both TrackEvent and the listener remains with the caller.
  TrackEventHandle(TrackEvent* event,
                   IncrementalState* incremental_state,
                   CompletionListener* listener)
      : event_(event),
        incremental_state_(incremental_state),
        listener_(listener) {}

  // Creates an invalid handle.
  TrackEventHandle() : TrackEventHandle(nullptr, nullptr, nullptr) {}

  ~TrackEventHandle() {
    if (listener_)
      listener_->OnTrackEventCompleted();
  }

  explicit operator bool() const { return event_; }
  TrackEvent& operator*() const { return *event_; }
  TrackEvent* operator->() const { return event_; }
  TrackEvent* get() const { return event_; }

  IncrementalState* incremental_state() const { return incremental_state_; }

 private:
  TrackEvent* event_;
  IncrementalState* incremental_state_;
  CompletionListener* listener_;
};

using PrepareTrackEventFunction = TrackEventHandle (*)(TraceEvent*);

// Embedder should call this (only once) to set the callback invoked when a
// typed event should be emitted. The callback function may be executed on any
// thread. Implemented in typed_macros_internal.h.
BASE_EXPORT void EnableTypedTraceEvents(
    PrepareTrackEventFunction typed_event_callback);

BASE_EXPORT void ResetTypedTraceEventsForTesting();

}  // namespace trace_event
}  // namespace base

#endif  // BASE_TRACE_EVENT_TYPED_MACROS_EMBEDDER_SUPPORT_H_
