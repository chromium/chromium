// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/js_sandbox/service/message_event.h"

#include <cstdint>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/check.h"
#include "gin/converter.h"
#include "gin/object_template_builder.h"
#include "v8/include/v8-cppgc.h"

namespace android_webview {
gin::ObjectTemplateBuilder MessageEvent::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<MessageEvent>::GetObjectTemplateBuilder(isolate)
      .SetProperty("data", &MessageEvent::GetData);
}

const gin::WrapperInfo* MessageEvent::wrapper_info() const {
  return &kWrapperInfo;
}

MessageEvent::MessageEvent(v8::Isolate* isolate, v8::Local<v8::Value> data)
    : data_(isolate, data) {}
MessageEvent::~MessageEvent() = default;

// Returns the data of the event.
v8::Local<v8::Value> MessageEvent::GetData(v8::Isolate* isolate) const {
  return data_.Get(isolate);
}

// CppGC Tracing method implementation
void MessageEvent::Trace(cppgc::Visitor* visitor) const {
  visitor->Trace(data_);
  gin::Wrappable<MessageEvent>::Trace(visitor);
}
}  // namespace android_webview
