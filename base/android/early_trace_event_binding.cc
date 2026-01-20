// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/early_trace_event_binding.h"

#include <stdint.h>

#include "base/android/jni_string.h"
#include "base/android/trace_event_binding.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/tracing_buildflags.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/tasks_jni/EarlyTraceEvent_jni.h"

namespace base {
namespace android {

static void JNI_EarlyTraceEvent_RecordEarlyBeginEvent(JNIEnv* env,
                                                      std::string& name,
                                                      int64_t time_ns,
                                                      int32_t thread_id,
                                                      int64_t thread_time_ms) {
  auto t = perfetto::ThreadTrack::ForThread(thread_id);
  TRACE_EVENT_BEGIN(internal::kJavaTraceCategory, perfetto::DynamicString{name},
                    t, TimeTicks::FromJavaNanoTime(time_ns));
}

static void JNI_EarlyTraceEvent_RecordEarlyEndEvent(JNIEnv* env,
                                                    std::string& name,
                                                    int64_t time_ns,
                                                    int32_t thread_id,
                                                    int64_t thread_time_ms) {
  auto t = perfetto::ThreadTrack::ForThread(thread_id);
  TRACE_EVENT_END(internal::kJavaTraceCategory, t,
                  TimeTicks::FromJavaNanoTime(time_ns));
}

static void JNI_EarlyTraceEvent_RecordEarlyToplevelBeginEvent(
    JNIEnv* env,
    std::string& name,
    int64_t time_ns,
    int32_t thread_id) {
  auto t = perfetto::ThreadTrack::ForThread(thread_id);
  TRACE_EVENT_BEGIN(internal::kToplevelTraceCategory,
                    perfetto::DynamicString{name}, t,
                    TimeTicks::FromJavaNanoTime(time_ns));
}

static void JNI_EarlyTraceEvent_RecordEarlyToplevelEndEvent(JNIEnv* env,
                                                            std::string& name,
                                                            int64_t time_ns,
                                                            int32_t thread_id) {
  auto t = perfetto::ThreadTrack::ForThread(thread_id);
  TRACE_EVENT_END(internal::kToplevelTraceCategory, t,
                  TimeTicks::FromJavaNanoTime(time_ns));
}

static void JNI_EarlyTraceEvent_RecordEarlyAsyncBeginEvent(JNIEnv* env,
                                                           std::string& name,
                                                           int64_t id,
                                                           int64_t time_ns) {
  TRACE_EVENT_BEGIN(
      internal::kJavaTraceCategory, nullptr,
      perfetto::Track(static_cast<uint64_t>(id)),
      TimeTicks::FromJavaNanoTime(time_ns),
      [&](::perfetto::EventContext& ctx) { ctx.event()->set_name(name); });
}

static void JNI_EarlyTraceEvent_RecordEarlyAsyncEndEvent(JNIEnv* env,
                                                         int64_t id,
                                                         int64_t time_ns) {
  TRACE_EVENT_END(internal::kJavaTraceCategory,
                  perfetto::Track(static_cast<uint64_t>(id)));
}

bool GetBackgroundStartupTracingFlagFromJava() {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  return base::android::Java_EarlyTraceEvent_getBackgroundStartupTracingFlag(
      env);
}

void SetBackgroundStartupTracingFlag(bool enabled) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  base::android::Java_EarlyTraceEvent_setBackgroundStartupTracingFlag(env,
                                                                      enabled);
}

}  // namespace android
}  // namespace base

DEFINE_JNI(EarlyTraceEvent)
