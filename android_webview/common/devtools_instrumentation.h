// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_COMMON_DEVTOOLS_INSTRUMENTATION_H_
#define ANDROID_WEBVIEW_COMMON_DEVTOOLS_INSTRUMENTATION_H_

#include "base/trace_event/trace_event.h"

namespace android_webview {
namespace devtools_instrumentation {

namespace internal {
constexpr const char* Category() {
  // Declared as a constexpr function to have an external linkage and to be
  // known at compile-time.
  return "Java,devtools,disabled-by-default-devtools.timeline";
}
const char kEmbedderCallback[] = "EmbedderCallback";
const char kCallbackNameArgument[] = "callbackName";
}  // namespace internal

class ScopedEmbedderCallbackTask {
 public:
  explicit ScopedEmbedderCallbackTask(const char* callback_name) {
    TRACE_EVENT_BEGIN1(internal::Category(), internal::kEmbedderCallback,
                       internal::kCallbackNameArgument, callback_name);
  }

  ScopedEmbedderCallbackTask(const ScopedEmbedderCallbackTask&) = delete;
  ScopedEmbedderCallbackTask& operator=(const ScopedEmbedderCallbackTask&) =
      delete;

  ~ScopedEmbedderCallbackTask() {
    TRACE_EVENT_END0(internal::Category(), internal::kEmbedderCallback);
  }
};

}  // namespace devtools_instrumentation
}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_COMMON_DEVTOOLS_INSTRUMENTATION_H_
