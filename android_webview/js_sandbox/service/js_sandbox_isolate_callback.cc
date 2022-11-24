// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/js_sandbox/service/js_sandbox_isolate_callback.h"

#include <jni.h>
#include <sstream>

#include "android_webview/js_sandbox/js_sandbox_jni_headers/JsSandboxIsolateCallback_jni.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"

namespace android_webview {

JsSandboxIsolateCallback::JsSandboxIsolateCallback(
    base::android::ScopedJavaGlobalRef<jobject>&& callback)
    : callback_(std::move(callback)) {
  CHECK(callback_) << "JsSandboxIsolateCallback java object is null";
}

JsSandboxIsolateCallback::~JsSandboxIsolateCallback() = default;

void JsSandboxIsolateCallback::ReportResult(const std::string& result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> java_string_result =
      base::android::ConvertUTF8ToJavaString(env, result);
  Java_JsSandboxIsolateCallback_onResult(env, UseCallback(),
                                         java_string_result);
}

void JsSandboxIsolateCallback::ReportJsEvaluationError(
    const std::string& error) {
  ReportError(ErrorType::kJsEvaluationError, error);
}

void JsSandboxIsolateCallback::ReportMemoryLimitExceededError(
    const uint64_t memory_limit,
    const uint64_t heap_usage) {
  std::ostringstream details;
  details << "Memory limit exceeded.\n";
  if (memory_limit > 0) {
    details << "Memory limit: " << memory_limit << " bytes\n";
  } else {
    details << "Memory limit not explicitly configured\n";
  }
  details << "Heap usage: " << heap_usage << " bytes\n";
  ReportError(ErrorType::kMemoryLimitExceeded, details.str());
}

void JsSandboxIsolateCallback::ReportError(const ErrorType error_type,
                                           const std::string& error) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> java_string_error =
      base::android::ConvertUTF8ToJavaString(env, error);
  Java_JsSandboxIsolateCallback_onError(
      env, UseCallback(), static_cast<jint>(error_type), java_string_error);
}

base::android::ScopedJavaGlobalRef<jobject>
JsSandboxIsolateCallback::UseCallback() {
  CHECK(callback_) << "Double use of isolate callback";
  // Move resets callback_ to null
  return std::move(callback_);
}

}  // namespace android_webview
