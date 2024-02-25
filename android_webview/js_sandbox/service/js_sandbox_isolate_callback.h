// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_JS_SANDBOX_SERVICE_JS_SANDBOX_ISOLATE_CALLBACK_H_
#define ANDROID_WEBVIEW_JS_SANDBOX_SERVICE_JS_SANDBOX_ISOLATE_CALLBACK_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"

namespace android_webview {

class JsSandboxIsolateCallback final
    : public base::RefCounted<JsSandboxIsolateCallback> {
 public:
  enum class ErrorType {
    kJsEvaluationError = 0,
    kMemoryLimitExceeded = 1,
    kFileDescriptorIOFailedError = 2,
  };

  explicit JsSandboxIsolateCallback(
      base::android::ScopedJavaGlobalRef<jobject>&& callback,
      bool use_fd);
  JsSandboxIsolateCallback(const JsSandboxIsolateCallback&) = delete;
  JsSandboxIsolateCallback& operator=(const JsSandboxIsolateCallback&) = delete;

  void ReportResult(const std::string& result);
  void ReportError(ErrorType error_type, const std::string& error);
  void ReportJsEvaluationError(const std::string& error);
  void ReportFileDescriptorIOFailedError(const std::string& error);
  // Report that the isolate has exceeded its memory limit, with various stats.
  //
  // memory_limit == 0 indicates that no explicit limit was configured.
  //
  // v8_heap_usage describes V8-internal (V8 heap) memory usage before the
  // failed allocation. non_v8_heap_usage describes memory usage external to the
  // V8 heap.
  void ReportMemoryLimitExceededError(uint64_t memory_limit,
                                      uint64_t v8_heap_usage,
                                      uint64_t non_v8_heap_usage);

 private:
  friend class base::RefCounted<JsSandboxIsolateCallback>;
  ~JsSandboxIsolateCallback();

  base::android::ScopedJavaGlobalRef<jobject> UseCallback();

  // Access this via UseCallback() to ensure the callback isn't used multiple
  // times. This value with be reset (null) when it is used.
  base::android::ScopedJavaGlobalRef<jobject> callback_;
  // If true, |callback_| is of Java type JsSandboxIsolateFdCallback.
  // If false, |callback_| is of Java type JsSandboxIsolateCallback.
  bool use_fd;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_JS_SANDBOX_SERVICE_JS_SANDBOX_ISOLATE_CALLBACK_H_
