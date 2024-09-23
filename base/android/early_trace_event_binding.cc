// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/early_trace_event_binding.h"

#include <stdint.h>

#include "base/android/jni_string.h"
#include "base/android/trace_event_binding.h"
#include "base/time/time.h"
#include "base/trace_event/base_tracing.h"
#include "base/tracing_buildflags.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/tasks_jni/EarlyTraceEvent_jni.h"

namespace base {
namespace android {

static void JNI_EarlyTraceEvent_RecordEarlyBeginEvent(
    JNIEnv* env,
    const JavaParamRef<jstring>& jname,
    jlong time_ns,
    jint thread_id,
    jlong thread_time_ms) {
#if BUILDFLAG(ENABLE_BASE_TRACING)
  std::string name = ConvertJavaStringToUTF8(env, jname);

  static const unsigned char* category_group_enabled =
      TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(internal::kJavaTraceCategory);
  trace_event_internal::AddTraceEventWithThreadIdAndTimestamps(
      TRACE_EVENT_PHASE_BEGIN, category_group_enabled, name.c_str(),
      /*scope=*/nullptr, trace_event_internal::kNoId, thread_id,
      TimeTicks::FromJavaNanoTime(time_ns),
      ThreadTicks() + Milliseconds(thread_time_ms),
      TRACE_EVENT_FLAG_JAVA_STRING_LITERALS | TRACE_EVENT_FLAG_COPY);
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)
}

static void JNI_EarlyTraceEvent_RecordEarlyEndEvent(
    JNIEnv* env,
    const JavaParamRef<jstring>& jname,
    jlong time_ns,
    jint thread_id,
    jlong thread_time_ms) {
#if BUILDFLAG(ENABLE_BASE_TRACING)
  std::string name = ConvertJavaStringToUTF8(env, jname);

  static const unsigned char* category_group_enabled =
      TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(internal::kJavaTraceCategory);
  trace_event_internal::AddTraceEventWithThreadIdAndTimestamps(
      TRACE_EVENT_PHASE_END, category_group_enabled, name.c_str(),
      /*scope=*/nullptr, trace_event_internal::kNoId, thread_id,
      TimeTicks::FromJavaNanoTime(time_ns),
      ThreadTicks() + Milliseconds(thread_time_ms),
      TRACE_EVENT_FLAG_JAVA_STRING_LITERALS | TRACE_EVENT_FLAG_COPY);
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)
}

static void JNI_EarlyTraceEvent_RecordEarlyToplevelBeginEvent(
    JNIEnv* env,
    const JavaParamRef<jstring>& jname,
    jlong time_ns,
    jint thread_id,
    jlong thread_time_ms) {
#if BUILDFLAG(ENABLE_BASE_TRACING)
  std::string name = ConvertJavaStringToUTF8(env, jname);

  static const unsigned char* category_group_enabled =
      TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(
          internal::kToplevelTraceCategory);
  trace_event_internal::AddTraceEventWithThreadIdAndTimestamps(
      TRACE_EVENT_PHASE_BEGIN, category_group_enabled, name.c_str(),
      /*scope=*/nullptr, trace_event_internal::kNoId, thread_id,
      TimeTicks::FromJavaNanoTime(time_ns),
      ThreadTicks() + Milliseconds(thread_time_ms),
      TRACE_EVENT_FLAG_JAVA_STRING_LITERALS | TRACE_EVENT_FLAG_COPY);
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)
}

static void JNI_EarlyTraceEvent_RecordEarlyToplevelEndEvent(
    JNIEnv* env,
    const JavaParamRef<jstring>& jname,
    jlong time_ns,
    jint thread_id,
    jlong thread_time_ms) {
#if BUILDFLAG(ENABLE_BASE_TRACING)
  std::string name = ConvertJavaStringToUTF8(env, jname);

  static const unsigned char* category_group_enabled =
      TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(
          internal::kToplevelTraceCategory);
  trace_event_internal::AddTraceEventWithThreadIdAndTimestamps(
      TRACE_EVENT_PHASE_END, category_group_enabled, name.c_str(),
      /*scope=*/nullptr, trace_event_internal::kNoId, thread_id,
      TimeTicks::FromJavaNanoTime(time_ns),
      ThreadTicks() + Milliseconds(thread_time_ms),
      TRACE_EVENT_FLAG_JAVA_STRING_LITERALS | TRACE_EVENT_FLAG_COPY);
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)
}

static void JNI_EarlyTraceEvent_RecordEarlyAsyncBeginEvent(
    JNIEnv* env,
    const JavaParamRef<jstring>& jname,
    jlong id,
    jlong time_ns) {
  TRACE_EVENT_BEGIN(internal::kJavaTraceCategory, nullptr,
                    perfetto::Track(static_cast<uint64_t>(id)),
                    TimeTicks::FromJavaNanoTime(time_ns),
                    [&](::perfetto::EventContext& ctx) {
                      std::string name = ConvertJavaStringToUTF8(env, jname);
                      ctx.event()->set_name(name.c_str());
                    });
}

static void JNI_EarlyTraceEvent_RecordEarlyAsyncEndEvent(JNIEnv* env,
                                                         jlong id,
                                                         jlong time_ns) {
  TRACE_EVENT_END(internal::kJavaTraceCategory,
                  perfetto::Track(static_cast<uint64_t>(id)));
}

bool GetBackgroundStartupTracingFlag() {
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
