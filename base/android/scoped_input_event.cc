// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/scoped_input_event.h"

#include "base/android/build_info.h"
#include "base/check.h"
#include "base/notreached.h"

namespace base::android {

// The class calls AInputEvent_release which was added only in Android S(31).
#ifndef SCOPED_INPUT_EVENT_MIN_API
#define SCOPED_INPUT_EVENT_MIN_API 31
#endif

ScopedInputEvent::ScopedInputEvent(const AInputEvent* event) {
  CHECK(base::android::BuildInfo::GetInstance()->sdk_int() >=
        SCOPED_INPUT_EVENT_MIN_API);
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
