// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_JS_SANDBOX_SERVICE_JS_SANDBOX_MESSAGE_PORT_H_
#define ANDROID_WEBVIEW_JS_SANDBOX_SERVICE_JS_SANDBOX_MESSAGE_PORT_H_

#include <string>

#include "android_webview/js_sandbox/service/js_sandbox_isolate.h"
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "gin/wrappable.h"
#include "v8/include/v8-traced-handle.h"

namespace gin {
class Arguments;
}  // namespace gin

namespace android_webview {
class JsSandboxIsolate;

class JsSandboxMessagePort : public gin::Wrappable<JsSandboxMessagePort> {
 public:
  static constexpr gin::WrapperInfo kWrapperInfo = {{gin::kEmbedderNativeGin},
                                                    gin::kJsSandboxMessagePort};

  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  const gin::WrapperInfo* wrapper_info() const override;

  // CppGC tracing.
  void Trace(cppgc::Visitor* visitor) const override;

  static JsSandboxMessagePort* Create(
      JsSandboxIsolate* js_sandbox_isolate,
      const base::android::JavaParamRef<jobject>& j_message_port);

  explicit JsSandboxMessagePort(
      JsSandboxIsolate* js_sandbox_isolate,
      const base::android::JavaParamRef<jobject>& j_message_port);
  ~JsSandboxMessagePort() override;

  JsSandboxMessagePort(const JsSandboxMessagePort&) = delete;
  JsSandboxMessagePort& operator=(const JsSandboxMessagePort&) = delete;

  // JNI entry point for receiving string messages.
  // Called from a Binder thread.
  // Offloads string handling to the isolate thread.
  void HandleString(JNIEnv* env, std::string string);

  // JNI entry point for receiving array buffer messages.
  // Called from a Binder thread.
  // Offloads array buffer handling to the isolate thread.
  void HandleArrayBuffer(
      JNIEnv* env,
      const base::android::JavaParamRef<jbyteArray>& j_array_buffer);

  // Closes the message port.
  //
  // Once the port is closed, received messages are silently discarded.
  // Closing the port notifies the remote port that this end of the channel has
  // been closed.
  //
  // Called by the API or on isolate/sandbox close/death.
  //
  // Called on the isolate thread.
  void Close();

 private:
  // Send a message to the other side of the message channel. This method does
  // not block the other side from receiving messages.
  //
  // There are no implicit conversions, doing `postMessage(52)` instead of
  // `postMessage("52")` will throw an error.
  //
  // Passing unsupported arguments to the method will result in an exception.
  //
  // Does not support unpaired surrogate strings. Any string containing unpaired
  // surrogates will result in transforming the pairs in unicode blocks.
  //
  // Called on the isolate thread.
  void PostMessage(gin::Arguments* args);

  // See SetOnMessage. Initially null.
  //
  // Called on the isolate thread.
  v8::Local<v8::Value> GetOnMessage() const;

  // Set a message handling callback.
  //
  // Setting onmessage to something other than a function (including null or
  // undefined) will result in messages being silently discarded. This behaviour
  // is consistent even if the property is set from its default null value to an
  // explicit null.
  //
  // If the size of the message exceeds the isolate's memory budget, the isolate
  // (and sandbox) will be killed.
  //
  // Called on the isolate thread.
  void SetOnMessage(gin::Arguments* args);

  // Executes onmessage callback with the given String as a MessageEvent.
  void HandleStringOnIsolateThread(std::string string);
  // Executes onmessage callback with the given ArrayBuffer as a MessageEvent.
  void HandleArrayBufferOnIsolateThread(
      base::android::ScopedJavaGlobalRef<jbyteArray> j_array_buffer);

  // Java-side JsSandboxMessagePort object corresponding to this message port.
  base::android::ScopedJavaGlobalRef<jobject> j_js_sandbox_message_port_;
  // Called for each message received.
  v8::TracedReference<v8::Function> onmessage_;
  // Used to interact with the isolate that owns this message port.
  raw_ptr<JsSandboxIsolate> js_sandbox_isolate_;
};
}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_JS_SANDBOX_SERVICE_JS_SANDBOX_MESSAGE_PORT_H_
