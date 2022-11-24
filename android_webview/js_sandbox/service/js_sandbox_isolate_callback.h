// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_JS_SANDBOX_SERVICE_JS_SANDBOX_ISOLATE_CALLBACK_H_
#define ANDROID_WEBVIEW_JS_SANDBOX_SERVICE_JS_SANDBOX_ISOLATE_CALLBACK_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"

namespace android_webview {

class JsSandboxIsolateCallback final {
 public:
  enum class ErrorType {
    kJsEvaluationError = 0,
    kMemoryLimitExceeded = 1,
  };

  explicit JsSandboxIsolateCallback(
      base::android::ScopedJavaGlobalRef<jobject>&& callback);
  JsSandboxIsolateCallback(const JsSandboxIsolateCallback&) = delete;
  JsSandboxIsolateCallback& operator=(const JsSandboxIsolateCallback&) = delete;
  ~JsSandboxIsolateCallback();

  void ReportResult(const std::string& result);
  void ReportJsEvaluationError(const std::string& error);
  // Report that the isolate has exceeded its memory limit, with various stats.
  //
  // memory_limit == 0 indicates that no explicit limit was configured.
  //
  // heap_usage describes memory usage before the failed allocation.
  void ReportMemoryLimitExceededError(uint64_t memory_limit,
                                      uint64_t heap_usage);

 private:
  void ReportError(ErrorType error_type, const std::string& error);

  base::android::ScopedJavaGlobalRef<jobject> callback_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_JS_SANDBOX_SERVICE_JS_SANDBOX_ISOLATE_CALLBACK_H_
