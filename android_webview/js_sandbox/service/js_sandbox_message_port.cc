// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/js_sandbox/service/js_sandbox_message_port.h"

#include <algorithm>
#include <string>
#include <utility>

#include "android_webview/js_sandbox/service/message_event.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "gin/arguments.h"
#include "gin/converter.h"
#include "gin/function_template.h"
#include "gin/object_template_builder.h"
#include "gin/public/context_holder.h"
#include "v8/include/cppgc/allocation.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-cppgc.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-locker.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-primitive.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/js_sandbox/js_sandbox_jni_headers/JsSandboxMessagePort_jni.h"

namespace android_webview {

namespace {

// Given a logical message size, return an adjusted memory size accounting for
// overheads.
size_t MemoryAllocationSize(size_t size) {
  // An empty message still has some overhead. If we specify a size of 1, it
  // will be rounded up to a whole allocation page size, which is a minimally
  // defensive minimum.
  return std::max(size, size_t{1});
}

}  // namespace

gin::ObjectTemplateBuilder JsSandboxMessagePort::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<JsSandboxMessagePort>::GetObjectTemplateBuilder(isolate)
      .SetMethod("postMessage", &JsSandboxMessagePort::PostMessage)
      .SetMethod("close", &JsSandboxMessagePort::Close)
      .SetProperty("onmessage", &JsSandboxMessagePort::GetOnMessage,
                   &JsSandboxMessagePort::SetOnMessage);
}

const gin::WrapperInfo* JsSandboxMessagePort::wrapper_info() const {
  return &kWrapperInfo;
}

void JsSandboxMessagePort::Trace(cppgc::Visitor* visitor) const {
  visitor->Trace(onmessage_);
  gin::Wrappable<JsSandboxMessagePort>::Trace(visitor);
}

JsSandboxMessagePort* JsSandboxMessagePort::Create(
    JsSandboxIsolate* js_sandbox_isolate,
    const base::android::JavaRef<jobject>& j_message_port) {
  v8::Isolate* isolate = js_sandbox_isolate->GetIsolate();
  JsSandboxMessagePort* message_port =
      cppgc::MakeGarbageCollected<JsSandboxMessagePort>(
          isolate->GetCppHeap()->GetAllocationHandle(), js_sandbox_isolate,
          j_message_port);
  return message_port;
}

JsSandboxMessagePort::JsSandboxMessagePort(
    JsSandboxIsolate* js_sandbox_isolate,
    const base::android::JavaRef<jobject>& j_message_port)
    : js_sandbox_isolate_(js_sandbox_isolate) {
  JNIEnv* env = base::android::AttachCurrentThread();
  intptr_t native_js_sandbox_message_port = reinterpret_cast<intptr_t>(this);
  j_js_sandbox_message_port_ = Java_JsSandboxMessagePort_create(
      env, j_message_port, native_js_sandbox_message_port);
}

JsSandboxMessagePort::~JsSandboxMessagePort() {
  Close();
}

void JsSandboxMessagePort::HandleString(JNIEnv* env,
                                        std::string string,
                                        int64_t size) {
  // TODO(crbug.com/450579523): Post tasks via control thread, so that the tasks
  // are cancellable (like evaluateJavaScriptAsync).
  js_sandbox_isolate_->GetIsolateTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&JsSandboxMessagePort::HandleStringOnIsolateThread,
                     base::Unretained(this), std::move(string),
                     base::checked_cast<size_t>(size)));
}

void JsSandboxMessagePort::HandleArrayBuffer(
    JNIEnv* env,
    const base::android::JavaRef<jbyteArray>& j_array_buffer,
    int64_t size) {
  base::android::ScopedJavaGlobalRef<jbyteArray> j_array_buffer_global(
      j_array_buffer);
  // TODO(crbug.com/450579523): Post tasks via control thread, so that the tasks
  // are cancellable (like evaluateJavaScriptAsync).
  js_sandbox_isolate_->GetIsolateTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&JsSandboxMessagePort::HandleArrayBufferOnIsolateThread,
                     base::Unretained(this), std::move(j_array_buffer_global),
                     base::checked_cast<size_t>(size)));
}

void JsSandboxMessagePort::PostMessage(gin::Arguments* args) {
  JNIEnv* env = base::android::AttachCurrentThread();
  v8::Isolate* isolate = js_sandbox_isolate_->GetIsolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Value> value;
  if (args->Length() != 1) {
    args->ThrowTypeError("postMessage requires exactly one argument.");
    return;
  }
  if (!args->GetNext(&value)) {
    args->ThrowTypeError("Invalid argument type.");
    return;
  }

  if (value->IsString()) {
    std::string string;
    if (!gin::Converter<std::string>::FromV8(isolate, value, &string)) {
      args->ThrowTypeError("Failed to convert argument to string");
      return;
    }

    android_webview::Java_JsSandboxMessagePort_postString(
        env, j_js_sandbox_message_port_, string);
  } else if (value->IsArrayBuffer()) {
    v8::Local<v8::ArrayBuffer> v8_array_buffer = value.As<v8::ArrayBuffer>();
    auto backing_store = v8_array_buffer->GetBackingStore();

    // Not going to get out of sync or become invalid.
    // SAFETY: This runs on isolate thread, and length is handled by V8.
    base::android::ScopedJavaLocalRef<jbyteArray> j_array_buffer =
        UNSAFE_BUFFERS(base::android::ToJavaByteArray(
            env, static_cast<const uint8_t*>(backing_store->Data()),
            backing_store->ByteLength()));

    android_webview::Java_JsSandboxMessagePort_postArrayBuffer(
        env, j_js_sandbox_message_port_, j_array_buffer);
  } else {
    args->ThrowTypeError("Unsupported message type.");
  }
}

v8::Local<v8::Value> JsSandboxMessagePort::GetOnMessage() const {
  v8::Isolate* isolate = js_sandbox_isolate_->GetIsolate();
  v8::Local<v8::Function> onmessage = onmessage_.Get(isolate);
  if (onmessage.IsEmpty()) {
    return v8::Null(isolate);
  }
  return onmessage;
}

void JsSandboxMessagePort::SetOnMessage(gin::Arguments* args) {
  v8::Isolate* isolate = js_sandbox_isolate_->GetIsolate();

  v8::Local<v8::Value> value;
  if (!args->GetNext(&value)) {
    args->ThrowTypeError("Invalid argument type.");
    return;
  }
  if (!value->IsFunction()) {
    onmessage_.Reset();
    return;
  }

  v8::Local<v8::Function> onmessage = value.As<v8::Function>();
  onmessage_.Reset(isolate, onmessage);
}

void JsSandboxMessagePort::Close() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_JsSandboxMessagePort_close(env, j_js_sandbox_message_port_);
}

bool JsSandboxMessagePort::TryAllocateMemoryBudget(JNIEnv* env, int64_t size) {
  CHECK_GE(size, 0);
  if (base::as_unsigned(size) > SIZE_MAX) {
    return false;
  }

  // TODO(b/435619571):
  //  This doesn't attempt any garbage collection if the allocation fails, and
  //  cannot easily do so due us not being on the isolate thread yet. This could
  //  lead to unnecessary crashes if there are, for example, externally
  //  allocated arraybuffers that are no longer reachable that could be
  //  reclaimed. It's unclear how much of a problem this will be in practice,
  //  because V8 should theoretically perform proactive GCs, even for external
  //  memory allocations. However, this may not be perfect.
  //
  //  We could consider multiple strategies if this is a problem, including more
  //  aggressive GCs, reserving a separate memory budget, or trying to block
  //  this synchronous call on a cross-thread garbage collection if desperate.
  if (!js_sandbox_isolate_->GetMemoryBudget()->Allocate(
          MemoryAllocationSize(base::checked_cast<size_t>(size)))) {
    js_sandbox_isolate_->ExternalMemoryLimitExceeded();
    return false;
  }

  return true;
}

void JsSandboxMessagePort::HandleStringOnIsolateThread(std::string string,
                                                       size_t size) {
  v8::Isolate* isolate = js_sandbox_isolate_->GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      js_sandbox_isolate_->GetContextHolder()->context();
  v8::Context::Scope scope(context);

  v8::Local<v8::String> v8_string =
      v8::String::NewFromUtf8(isolate, string.data(),
                              v8::NewStringType::kNormal, string.length())
          .ToLocalChecked();

  MessageEvent* event = cppgc::MakeGarbageCollected<MessageEvent>(
      isolate->GetCppHeap()->GetAllocationHandle(), isolate, v8_string);
  v8::Local<v8::Value> argv[] = {event->GetWrapper(isolate).ToLocalChecked()};

  // Release the memory held by the 'string' parameter.
  // Swapping with a temporary empty string forces deallocation
  // of the buffer originally held by 'string'.
  std::string().swap(string);
  js_sandbox_isolate_->GetMemoryBudget()->Free(MemoryAllocationSize(size));

  v8::Local<v8::Function> onmessage = onmessage_.Get(isolate);
  if (onmessage.IsEmpty()) {
    return;
  }

  std::ignore = onmessage->Call(isolate->GetCurrentContext(),
                                GetWrapper(isolate).ToLocalChecked(), 1, argv);
}

void JsSandboxMessagePort::HandleArrayBufferOnIsolateThread(
    base::android::ScopedJavaGlobalRef<jbyteArray> j_array_buffer,
    size_t size) {
  JNIEnv* env = base::android::AttachCurrentThread();
  v8::Isolate* isolate = js_sandbox_isolate_->GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      js_sandbox_isolate_->GetContextHolder()->context();
  v8::Context::Scope scope(context);

  jsize length = env->GetArrayLength(j_array_buffer.obj());
  // This may OOM the isolate if it does not have enough memory
  v8::Local<v8::ArrayBuffer> v8_array_buffer =
      v8::ArrayBuffer::New(isolate, length);
  void* buffer_data = v8_array_buffer->GetBackingStore()->Data();
  env->GetByteArrayRegion(j_array_buffer.obj(), 0, length,
                          static_cast<int8_t*>(buffer_data));

  // Release the global reference to the Java byte array.
  j_array_buffer.Reset();
  js_sandbox_isolate_->GetMemoryBudget()->Free(MemoryAllocationSize(size));

  MessageEvent* event = cppgc::MakeGarbageCollected<MessageEvent>(
      isolate->GetCppHeap()->GetAllocationHandle(), isolate, v8_array_buffer);
  v8::Local<v8::Value> argv[] = {event->GetWrapper(isolate).ToLocalChecked()};

  v8::Local<v8::Function> onmessage = onmessage_.Get(isolate);
  if (onmessage.IsEmpty()) {
    return;
  }

  std::ignore = onmessage->Call(isolate->GetCurrentContext(),
                                GetWrapper(isolate).ToLocalChecked(), 1, argv);
}

}  // namespace android_webview

DEFINE_JNI(JsSandboxMessagePort)
