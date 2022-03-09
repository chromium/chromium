// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include <set>

#include "base/android/jni_string.h"
#include "base/android/trace_event_binding.h"
#include "base/base_jni_headers/TraceEvent_jni.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/base_tracing.h"
#include "base/tracing_buildflags.h"

#if BUILDFLAG(ENABLE_BASE_TRACING)
#include "base/trace_event/trace_event_impl.h"  // no-presubmit-check
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)

namespace base {
namespace android {

#if BUILDFLAG(ENABLE_BASE_TRACING)

namespace {

constexpr const char kAndroidViewHierarchyTraceCategory[] =
    TRACE_DISABLED_BY_DEFAULT("android_view_hierarchy");
constexpr const char kAndroidViewHierarchyEventName[] = "AndroidView";

class TraceEnabledObserver
    : public trace_event::TraceLog::EnabledStateObserver {
 public:
  ~TraceEnabledObserver() override = default;

  // trace_event::TraceLog::EnabledStateObserver:
  void OnTraceLogEnabled() override {
    JNIEnv* env = base::android::AttachCurrentThread();
    base::android::Java_TraceEvent_setEnabled(env, true);
    if (base::trace_event::TraceLog::GetInstance()
            ->GetCurrentTraceConfig()
            .IsEventPackageNameFilterEnabled()) {
      base::android::Java_TraceEvent_setEventNameFilteringEnabled(env, true);
    }
  }
  void OnTraceLogDisabled() override {
    JNIEnv* env = base::android::AttachCurrentThread();
    base::android::Java_TraceEvent_setEnabled(env, false);
    base::android::Java_TraceEvent_setEventNameFilteringEnabled(env, false);
  }
};

}  // namespace

static void JNI_TraceEvent_RegisterEnabledObserver(JNIEnv* env) {
  bool enabled = trace_event::TraceLog::GetInstance()->IsEnabled();
  base::android::Java_TraceEvent_setEnabled(env, enabled);
  trace_event::TraceLog::GetInstance()->AddOwnedEnabledStateObserver(
      std::make_unique<TraceEnabledObserver>());
}

static void JNI_TraceEvent_StartATrace(
    JNIEnv* env,
    const JavaParamRef<jstring>& category_filter) {
  std::string category_filter_utf8 =
      ConvertJavaStringToUTF8(env, category_filter);
  base::trace_event::TraceLog::GetInstance()->StartATrace(category_filter_utf8);
}

static void JNI_TraceEvent_StopATrace(JNIEnv* env) {
  base::trace_event::TraceLog::GetInstance()->StopATrace();
}

static void JNI_TraceEvent_SetupATraceStartupTrace(
    JNIEnv* env,
    const JavaParamRef<jstring>& category_filter) {
  std::string category_filter_utf8 =
      ConvertJavaStringToUTF8(env, category_filter);
  base::trace_event::TraceLog::GetInstance()->SetupATraceStartupTrace(
      category_filter_utf8);
}

static jboolean JNI_TraceEvent_ViewHierarchyDumpEnabled(JNIEnv* env) {
  static const unsigned char* enabled =
      TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(
          kAndroidViewHierarchyTraceCategory);
  return *enabled;
}

static void JNI_TraceEvent_InitViewHierarchyDump(JNIEnv* env) {
  SCOPED_UMA_HISTOGRAM_TIMER("Tracing.ViewHierarchyDump.DumpDuration");
  TRACE_EVENT_INSTANT(
      kAndroidViewHierarchyTraceCategory, kAndroidViewHierarchyEventName,
      perfetto::Track::Global(0), [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* dump = event->set_android_view_dump();
        Java_TraceEvent_dumpViewHierarchy(env, reinterpret_cast<jlong>(dump));
      });
}

static jlong JNI_TraceEvent_StartActivityDump(JNIEnv* env,
                                              const JavaParamRef<jstring>& name,
                                              jlong dump_proto_ptr) {
  auto* dump = reinterpret_cast<perfetto::protos::pbzero::AndroidViewDump*>(
      dump_proto_ptr);
  auto* activity = dump->add_activity();
  activity->set_name(ConvertJavaStringToUTF8(env, name));
  return reinterpret_cast<jlong>(activity);
}

static void JNI_TraceEvent_AddViewDump(
    JNIEnv* env,
    jint id,
    jint parent_id,
    jboolean is_shown,
    jboolean is_dirty,
    const JavaParamRef<jstring>& class_name,
    const JavaParamRef<jstring>& resource_name,
    jlong activity_proto_ptr) {
  auto* activity = reinterpret_cast<perfetto::protos::pbzero::AndroidActivity*>(
      activity_proto_ptr);
  auto* view = activity->add_view();
  view->set_id(id);
  view->set_parent_id(parent_id);
  view->set_is_shown(is_shown);
  view->set_is_dirty(is_dirty);
  view->set_class_name(ConvertJavaStringToUTF8(env, class_name));
  view->set_resource_name(ConvertJavaStringToUTF8(env, resource_name));
}

#else  // BUILDFLAG(ENABLE_BASE_TRACING)

// Empty implementations when TraceLog isn't available.
static void JNI_TraceEvent_RegisterEnabledObserver(JNIEnv* env) {
  base::android::Java_TraceEvent_setEnabled(env, false);
  // This code should not be reached when base tracing is disabled. Calling
  // setEventNameFilteringEnabled to avoid "unused function" warning.
  base::android::Java_TraceEvent_setEventNameFilteringEnabled(env, false);
}
static void JNI_TraceEvent_StartATrace(JNIEnv* env,
                                       const JavaParamRef<jstring>&) {}
static void JNI_TraceEvent_StopATrace(JNIEnv* env) {}
static void JNI_TraceEvent_SetupATraceStartupTrace(
    JNIEnv* env,
    const JavaParamRef<jstring>&) {}
static jboolean JNI_TraceEvent_ViewHierarchyDumpEnabled(JNIEnv* env) {
  return false;
}
static void JNI_TraceEvent_InitViewHierarchyDump(JNIEnv* env) {
  DCHECK(false);
  // This code should not be reached when base tracing is disabled. Calling
  // dumpViewHierarchy to avoid "unused function" warning.
  Java_TraceEvent_dumpViewHierarchy(env, 0);
}
static jlong JNI_TraceEvent_StartActivityDump(JNIEnv* env,
                                              const JavaParamRef<jstring>& name,
                                              jlong dump_proto_ptr) {
  return 0;
}
static void JNI_TraceEvent_AddViewDump(
    JNIEnv* env,
    jint id,
    jint parent_id,
    jboolean is_shown,
    jboolean is_dirty,
    const JavaParamRef<jstring>& class_name,
    const JavaParamRef<jstring>& resource_name,
    jlong activity_proto_ptr) {}

#endif  // BUILDFLAG(ENABLE_BASE_TRACING)

namespace {

// Boilerplate for safely converting Java data to TRACE_EVENT data.
class TraceEventDataConverter {
 public:
  TraceEventDataConverter(JNIEnv* env, jstring jname, jstring jarg)
      : name_(ConvertJavaStringToUTF8(env, jname)),
        has_arg_(jarg != nullptr),
        arg_(jarg ? ConvertJavaStringToUTF8(env, jarg) : "") {}

  TraceEventDataConverter(const TraceEventDataConverter&) = delete;
  TraceEventDataConverter& operator=(const TraceEventDataConverter&) = delete;

  ~TraceEventDataConverter() = default;

  // Return saved values to pass to TRACE_EVENT macros.
  const char* name() { return name_.c_str(); }
  const char* arg_name() { return has_arg_ ? "arg" : nullptr; }
  const char* arg() { return has_arg_ ? arg_.c_str() : nullptr; }

 private:
  std::string name_;
  bool has_arg_;
  std::string arg_;
};

}  // namespace

static void JNI_TraceEvent_Instant(JNIEnv* env,
                                   const JavaParamRef<jstring>& jname,
                                   const JavaParamRef<jstring>& jarg) {
  TraceEventDataConverter converter(env, jname, jarg);
  if (converter.arg()) {
    TRACE_EVENT_INSTANT_WITH_FLAGS1(
        internal::kJavaTraceCategory, converter.name(),
        TRACE_EVENT_FLAG_JAVA_STRING_LITERALS | TRACE_EVENT_FLAG_COPY |
            TRACE_EVENT_SCOPE_THREAD,
        converter.arg_name(), converter.arg());
  } else {
    TRACE_EVENT_INSTANT_WITH_FLAGS0(
        internal::kJavaTraceCategory, converter.name(),
        TRACE_EVENT_FLAG_JAVA_STRING_LITERALS | TRACE_EVENT_FLAG_COPY |
            TRACE_EVENT_SCOPE_THREAD);
  }
}

static void JNI_TraceEvent_Begin(JNIEnv* env,
                                 const JavaParamRef<jstring>& jname,
                                 const JavaParamRef<jstring>& jarg) {
  TraceEventDataConverter converter(env, jname, jarg);
  if (converter.arg()) {
    TRACE_EVENT_BEGIN_WITH_FLAGS1(
        internal::kJavaTraceCategory, converter.name(),
        TRACE_EVENT_FLAG_JAVA_STRING_LITERALS | TRACE_EVENT_FLAG_COPY,
        converter.arg_name(), converter.arg());
  } else {
    TRACE_EVENT_BEGIN_WITH_FLAGS0(
        internal::kJavaTraceCategory, converter.name(),
        TRACE_EVENT_FLAG_JAVA_STRING_LITERALS | TRACE_EVENT_FLAG_COPY);
  }
}

static void JNI_TraceEvent_End(JNIEnv* env,
                               const JavaParamRef<jstring>& jname,
                               const JavaParamRef<jstring>& jarg) {
  TraceEventDataConverter converter(env, jname, jarg);
  if (converter.arg()) {
    TRACE_EVENT_END_WITH_FLAGS1(
        internal::kJavaTraceCategory, converter.name(),
        TRACE_EVENT_FLAG_JAVA_STRING_LITERALS | TRACE_EVENT_FLAG_COPY,
        converter.arg_name(), converter.arg());
  } else {
    TRACE_EVENT_END_WITH_FLAGS0(
        internal::kJavaTraceCategory, converter.name(),
        TRACE_EVENT_FLAG_JAVA_STRING_LITERALS | TRACE_EVENT_FLAG_COPY);
  }
}

static void JNI_TraceEvent_BeginToplevel(JNIEnv* env,
                                         const JavaParamRef<jstring>& jtarget) {
  std::string target = ConvertJavaStringToUTF8(env, jtarget);
  TRACE_EVENT_BEGIN_WITH_FLAGS0(
      internal::kToplevelTraceCategory, target.c_str(),
      TRACE_EVENT_FLAG_JAVA_STRING_LITERALS | TRACE_EVENT_FLAG_COPY);
}

static void JNI_TraceEvent_EndToplevel(JNIEnv* env,
                                       const JavaParamRef<jstring>& jtarget) {
  std::string target = ConvertJavaStringToUTF8(env, jtarget);
  TRACE_EVENT_END_WITH_FLAGS0(
      internal::kToplevelTraceCategory, target.c_str(),
      TRACE_EVENT_FLAG_JAVA_STRING_LITERALS | TRACE_EVENT_FLAG_COPY);
}

static void JNI_TraceEvent_StartAsync(JNIEnv* env,
                                      const JavaParamRef<jstring>& jname,
                                      jlong jid) {
  TraceEventDataConverter converter(env, jname, nullptr);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_FLAGS0(
      internal::kJavaTraceCategory, converter.name(), TRACE_ID_LOCAL(jid),
      TRACE_EVENT_FLAG_JAVA_STRING_LITERALS | TRACE_EVENT_FLAG_COPY);
}

static void JNI_TraceEvent_FinishAsync(JNIEnv* env,
                                       const JavaParamRef<jstring>& jname,
                                       jlong jid) {
  TraceEventDataConverter converter(env, jname, nullptr);
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_FLAGS0(
      internal::kJavaTraceCategory, converter.name(), TRACE_ID_LOCAL(jid),
      TRACE_EVENT_FLAG_JAVA_STRING_LITERALS | TRACE_EVENT_FLAG_COPY);
}

}  // namespace android
}  // namespace base
