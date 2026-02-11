// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_THREAD_TYPE_H_
#define BASE_TASK_THREAD_TYPE_H_

#include <string_view>

#include "base/base_export.h"

namespace base {

// Valid values for `thread_type` of Thread::Options, SimpleThread::Options,
// and SetCurrentThreadType(), listed in increasing order of importance.
//
// It is up to each platform-specific implementation what these translate to.
// Callers should avoid setting different ThreadTypes on different platforms
// (ifdefs) at all cost, instead the platform differences should be encoded in
// the platform-specific implementations. Some implementations may treat
// adjacent ThreadTypes in this enum as equivalent.
//
// Reach out to //base/task/OWNERS (scheduler-dev@chromium.org) before changing
// thread type assignments in your component, as such decisions affect the whole
// of Chrome.
//
// Refer to PlatformThreadTest.SetCurrentThreadTypeTest in
// platform_thread_unittest.cc for the most up-to-date state of each platform's
// handling of ThreadType.
enum class ThreadType : int {
  // Suitable for threads that have the least urgency and lowest priority, and
  // can be interrupted or delayed by other types.
  kBackground,
  // Suitable for threads that are less important than normal type, and can be
  // interrupted or delayed by threads with kDefault type.
  kUtility,
  // Default type. The thread priority or quality of service will be set to
  // platform default.
  kDefault,
  // Suitable for user visible  threads, ie. compositing and presenting
  // the foreground content.
  kPresentation,
  // Suitable for threads that handle audio processing, not including direct
  // audio rendering which should use kRealtimeAudio.
  kAudioProcessing,
  // Suitable for low-latency, glitch-resistant audio.
  kRealtimeAudio,
  kMaxValue = kRealtimeAudio,
};

BASE_EXPORT std::string_view ThreadTypeToString(ThreadType type);

}  // namespace base

#endif  // BASE_TASK_THREAD_TYPE_H_
