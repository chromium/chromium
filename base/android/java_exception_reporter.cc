// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/java_exception_reporter.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "build/robolectric_buildflags.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#if BUILDFLAG(IS_ROBOLECTRIC)
#include "base/base_robolectric_jni/JavaExceptionReporter_jni.h"  // nogncheck
#else
#include "base/base_jni/JavaExceptionReporter_jni.h"
#endif

using jni_zero::JavaParamRef;
using jni_zero::JavaRef;

namespace base {
namespace android {

namespace {

JavaExceptionCallback g_java_exception_callback;

using JavaExceptionFilter =
    base::RepeatingCallback<bool(const JavaRef<jthrowable>&)>;

LazyInstance<JavaExceptionFilter>::Leaky g_java_exception_filter =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

void InitJavaExceptionReporter() {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  // Since JavaExceptionReporter#installHandler will chain through to the
  // default handler, the default handler should cause a crash as if it's a
  // normal java exception. Prefer to crash the browser process in java rather
  // than native since for webview, the embedding app may have installed its
  // own JavaExceptionReporter handler and would expect it to be called.
  constexpr bool crash_after_report = false;
  SetJavaExceptionFilter(
      base::BindRepeating([](const JavaRef<jthrowable>&) { return true; }));
  Java_JavaExceptionReporter_installHandler(env, crash_after_report);
}

void InitJavaExceptionReporterForChildProcess() {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  constexpr bool crash_after_report = true;
  SetJavaExceptionFilter(
      base::BindRepeating([](const JavaRef<jthrowable>&) { return true; }));
  Java_JavaExceptionReporter_installHandler(env, crash_after_report);
}

void SetJavaExceptionFilter(JavaExceptionFilter java_exception_filter) {
  g_java_exception_filter.Get() = std::move(java_exception_filter);
}

void SetJavaExceptionCallback(JavaExceptionCallback callback) {
  DCHECK(!g_java_exception_callback || !callback);
  g_java_exception_callback = callback;
}

JavaExceptionCallback GetJavaExceptionCallback() {
  return g_java_exception_callback;
}

void SetJavaException(const char* exception) {
  // No need to print exception because they are already logged via
  // env->ExceptionDescribe() within jni_android.cc.
  if (g_java_exception_callback) {
    g_java_exception_callback(exception);
  }
}

void JNI_JavaExceptionReporter_ReportJavaException(
    JNIEnv* env,
    jboolean crash_after_report,
    const JavaParamRef<jthrowable>& e) {
  std::string exception_info = base::android::GetJavaExceptionInfo(env, e);
  bool should_report_exception = g_java_exception_filter.Get().Run(e);
  if (should_report_exception) {
    SetJavaException(exception_info.c_str());
  }
  if (crash_after_report) {
    LOG(ERROR) << exception_info;
    LOG(FATAL) << "Uncaught exception";
  }
  if (should_report_exception) {
    base::debug::DumpWithoutCrashing();
    SetJavaException(nullptr);
  }
}

void JNI_JavaExceptionReporter_ReportJavaStackTrace(JNIEnv* env,
                                                    std::string& stack_trace) {
  SetJavaException(stack_trace.c_str());
  base::debug::DumpWithoutCrashing();
  SetJavaException(nullptr);
}

}  // namespace android
}  // namespace base
