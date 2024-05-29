// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/js_sandbox/service/js_sandbox_isolate_callback.h"

#include <jni.h>
#include <sstream>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/files/file_util.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/js_sandbox/js_sandbox_jni_headers/JsSandboxIsolateCallback_jni.h"
#include "android_webview/js_sandbox/js_sandbox_jni_headers/JsSandboxIsolateFdCallback_jni.h"

namespace android_webview {

JsSandboxIsolateCallback::JsSandboxIsolateCallback(
    base::android::ScopedJavaGlobalRef<jobject>&& callback,
    bool use_fd)
    : callback_(std::move(callback)), use_fd(use_fd) {
  CHECK(callback_) << "JsSandboxIsolateCallback java object is null";
}

JsSandboxIsolateCallback::~JsSandboxIsolateCallback() = default;

void JsSandboxIsolateCallback::ReportResult(const std::string& result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (use_fd) {
    base::ScopedFD read_fd, write_fd;
    CHECK(base::CreatePipe(&read_fd, &write_fd));
    Java_JsSandboxIsolateFdCallback_onResult(
        env, UseCallback(), static_cast<jint>(read_fd.release()),
        static_cast<jint>(result.length()));
    // This might return false due to EPIPE if the client closes the fd without
    // reading from it. That is not an error for our use case.
    base::WriteFileDescriptor(write_fd.get(), std::move(result));
  } else {
    base::android::ScopedJavaLocalRef<jstring> java_string_result =
        base::android::ConvertUTF8ToJavaString(env, result);
    Java_JsSandboxIsolateCallback_onResult(env, UseCallback(),
                                           java_string_result);
  }
}

void JsSandboxIsolateCallback::ReportJsEvaluationError(
    const std::string& error) {
  ReportError(ErrorType::kJsEvaluationError, error);
}

void JsSandboxIsolateCallback::ReportFileDescriptorIOFailedError(
    const std::string& error) {
  ReportError(ErrorType::kFileDescriptorIOFailedError, error);
}

void JsSandboxIsolateCallback::ReportMemoryLimitExceededError(
    const uint64_t memory_limit,
    const uint64_t v8_heap_usage,
    const uint64_t non_v8_heap_usage) {
  std::ostringstream details;
  details << "Memory limit exceeded.\n";
  if (memory_limit > 0) {
    details << "Memory limit: " << memory_limit << " bytes\n";
  } else {
    details << "Memory limit not explicitly configured\n";
  }
  details << "V8 heap usage: " << v8_heap_usage << " bytes\n";
  details << "Non-V8 heap usage: " << non_v8_heap_usage << " bytes\n";
  ReportError(ErrorType::kMemoryLimitExceeded, details.str());
}

void JsSandboxIsolateCallback::ReportError(const ErrorType error_type,
                                           const std::string& error) {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (use_fd) {
    base::ScopedFD read_fd, write_fd;
    CHECK(base::CreatePipe(&read_fd, &write_fd));
    Java_JsSandboxIsolateFdCallback_onError(
        env, UseCallback(), static_cast<jint>(error_type),
        static_cast<jint>(read_fd.release()),
        static_cast<jint>(error.length()));
    // This might return false due to EPIPE if the client closes the fd without
    // reading from it. That is not an error for our use case.
    base::WriteFileDescriptor(write_fd.get(), std::move(error));
  } else {
    base::android::ScopedJavaLocalRef<jstring> java_string_error =
        base::android::ConvertUTF8ToJavaString(env, error);
    Java_JsSandboxIsolateCallback_onError(
        env, UseCallback(), static_cast<jint>(error_type), java_string_error);
  }
}

base::android::ScopedJavaGlobalRef<jobject>
JsSandboxIsolateCallback::UseCallback() {
  CHECK(callback_) << "Double use of isolate callback";
  // Move resets callback_ to null
  return std::move(callback_);
}

}  // namespace android_webview
