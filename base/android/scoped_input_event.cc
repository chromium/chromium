// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/scoped_input_event.h"

#include "base/android/android_info.h"
#include "base/check.h"
#include "base/notreached.h"

namespace base::android {

// The class calls AInputEvent_release which was added only in Android S(31).
#ifndef SCOPED_INPUT_EVENT_MIN_API
#define SCOPED_INPUT_EVENT_MIN_API 31
#endif

ScopedInputEvent::ScopedInputEvent(const AInputEvent* event) {
  CHECK(base::android::android_info::sdk_int() >= SCOPED_INPUT_EVENT_MIN_API);
  CHECK(event);
  a_input_event_ = event;
}

ScopedInputEvent::~ScopedInputEvent() {
  DestroyIfNeeded();
}

ScopedInputEvent::ScopedInputEvent(ScopedInputEvent&& other)
    : a_input_event_(other.a_input_event_) {
  other.a_input_event_ = nullptr;
}

ScopedInputEvent& ScopedInputEvent::operator=(ScopedInputEvent&& other) {
  if (this != &other) {
    DestroyIfNeeded();
    a_input_event_ = other.a_input_event_;
    other.a_input_event_ = nullptr;
  }
  return *this;
}

void ScopedInputEvent::WriteIntoTrace(
    perfetto::TracedProto<perfetto::protos::pbzero::EventForwarder> forwarder)
    const {
  if (!a_input_event_) {
    return;
  }

  const int history_size =
      static_cast<const int>(AMotionEvent_getHistorySize(a_input_event_));
  forwarder->set_history_size(history_size);

  forwarder->set_latest_time_ns(AMotionEvent_getEventTime(a_input_event_));
  if (history_size > 0) {
    forwarder->set_oldest_time_ns(AMotionEvent_getHistoricalEventTime(
        a_input_event_, /* history_index= */ 0));
  }
  forwarder->set_down_time_ns(AMotionEvent_getDownTime(a_input_event_));

  forwarder->set_x_pixel(
      AMotionEvent_getX(a_input_event_, /* pointer_index= */ 0));
  forwarder->set_y_pixel(
      AMotionEvent_getY(a_input_event_, /* pointer_index= */ 0));

  const int action =
      AMotionEvent_getAction(a_input_event_) & AMOTION_EVENT_ACTION_MASK;
  forwarder->set_action(
      static_cast<perfetto::protos::pbzero::EventForwarder::AMotionEventAction>(
          action));
}

void ScopedInputEvent::DestroyIfNeeded() {
  if (a_input_event_ == nullptr) {
    return;
  }
  // If check to suppress the compiler warning.
  if (__builtin_available(android SCOPED_INPUT_EVENT_MIN_API, *)) {
    AInputEvent_release(a_input_event_);
    a_input_event_ = nullptr;
    return;
  }
  NOTREACHED();
}

}  // namespace base::android
