// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_JS_SANDBOX_SERVICE_JS_SANDBOX_ISOLATE_H_
#define ANDROID_WEBVIEW_JS_SANDBOX_SERVICE_JS_SANDBOX_ISOLATE_H_

#include <memory>
#include <set>
#include <string>
#include <unordered_map>

#include "base/android/scoped_java_ref.h"
#include "base/compiler_specific.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "v8/include/v8-array-buffer.h"
#include "v8/include/v8-inspector.h"
#include "v8/include/v8-promise.h"

namespace base {
class CancelableTaskTracker;
class SequencedTaskRunner;
class SingleThreadTaskRunner;
}  // namespace base

namespace gin {
class Arguments;
class IsolateHolder;
class ContextHolder;
}  // namespace gin

namespace v8 {
class ObjectTemplate;
}  // namespace v8

namespace android_webview {

class FdWithLength;
class JsSandboxArrayBufferAllocator;
class JsSandboxIsolateCallback;

class JsSandboxIsolate {
 public:
  explicit JsSandboxIsolate(
      const base::android::JavaParamRef<jobject>& j_isolate_,
      size_t max_heap_size_bytes);
  ~JsSandboxIsolate();

  jboolean EvaluateJavascript(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& jcode,
      const base::android::JavaParamRef<jobject>& j_callback);
  jboolean EvaluateJavascriptWithFd(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const jint fd,
      const jlong length,
      const jlong offset,
      const base::android::JavaParamRef<jobject>& j_callback,
      const base::android::JavaParamRef<jobject>& pfd);
  void DestroyNative(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj);
  jboolean ProvideNamedData(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& obj,
                            const base::android::JavaParamRef<jstring>& jname,
                            const jint fd,
                            const jint length);
  // May enable or disable inspection, as needed.
  void SetConsoleEnabled(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj,
                         jboolean enable);

 private:
  class InspectorClient;

  void DeleteSelf();
  void InitializeIsolateOnThread();
  // Will enabled or disable inspection depending on whether any dynamic
  // features require it (for example, console logging).
  void EvaluateJavascriptOnThread(
      std::string code,
      scoped_refptr<JsSandboxIsolateCallback> callback);
  void PromiseFulfillCallback(scoped_refptr<JsSandboxIsolateCallback> callback,
                              gin::Arguments* args);
  void PromiseRejectCallback(scoped_refptr<JsSandboxIsolateCallback> callback,
                             gin::Arguments* args);

  void TerminateAndDestroy();
  void DestroyWhenPossible();
  void NotifyInitComplete();
  void CreateCancelableTaskTracker();
  void PostEvaluationToIsolateThread(
      std::string code,
      scoped_refptr<JsSandboxIsolateCallback> callback);
  void PostFileDescriptorReadToIsolateThread(
      int fd,
      int64_t length,
      int64_t offset,
      base::android::ScopedJavaGlobalRef<jobject> pfd,
      scoped_refptr<JsSandboxIsolateCallback> callback);
  void ReadFileDescriptorOnThread(
      int fd,
      int64_t length,
      int64_t offset,
      base::android::ScopedJavaGlobalRef<jobject> pfd,
      scoped_refptr<JsSandboxIsolateCallback> callback);
  void ReportFileDescriptorIOError(
      base::android::ScopedJavaGlobalRef<jobject> pfd,
      scoped_refptr<JsSandboxIsolateCallback> callback,
      std::string errorMessage);
  void ConvertPromiseToArrayBufferInThreadPool(
      base::ScopedFD fd,
      ssize_t length,
      std::string name,
      std::unique_ptr<v8::Global<v8::ArrayBuffer>> array_buffer,
      std::unique_ptr<v8::Global<v8::Promise::Resolver>> resolver,
      void* inner_buffer);
  void ConvertPromiseToArrayBufferInControlSequence(
      std::string name,
      std::unique_ptr<v8::Global<v8::ArrayBuffer>> array_buffer,
      std::unique_ptr<v8::Global<v8::Promise::Resolver>> resolver);
  void ConvertPromiseToFailureInControlSequence(
      std::string name,
      std::unique_ptr<v8::Global<v8::ArrayBuffer>> array_buffer,
      std::unique_ptr<v8::Global<v8::Promise::Resolver>> resolver,
      std::string reason);
  void ConvertPromiseToFailureInIsolateSequence(
      std::string name,
      std::unique_ptr<v8::Global<v8::ArrayBuffer>> array_buffer,
      std::unique_ptr<v8::Global<v8::Promise::Resolver>> resolver,
      std::string reason);
  void ConvertPromiseToArrayBufferInIsolateSequence(
      std::string name,
      std::unique_ptr<v8::Global<v8::ArrayBuffer>> array_buffer,
      std::unique_ptr<v8::Global<v8::Promise::Resolver>> resolver);

  void ConsumeNamedDataAsArrayBuffer(gin::Arguments* args);
  v8::Local<v8::ObjectTemplate> CreateAndroidNamespaceTemplate(
      v8::Isolate* isolate);

  // Must only be used from isolate thread
  [[noreturn]] static size_t NearHeapLimitCallback(void* data,
                                                   size_t current_heap_limit,
                                                   size_t initial_heap_limit);
  v8::MaybeLocal<v8::ArrayBuffer> tryAllocateArrayBuffer(size_t length);

  // Must only be used from isolate thread
  [[noreturn]] void MemoryLimitExceeded();
  // Must only be used from isolate thread
  void ReportOutOfMemory();
  [[noreturn]] void FreezeThread();

  void EnableOrDisableInspectorAsNeeded();
  void SetConsoleEnabledOnControlThread(bool enable);
  void SetConsoleEnabledOnIsolateThread(bool enable);

  // Remove a callback from the ongoing_evaluation_callbacks_ set.
  //
  // Returns the passed reference to allow chaining. (The caller must make sure
  // the reference continues to be valid if it is used.)
  //
  // Must only be used from isolate thread.
  const scoped_refptr<JsSandboxIsolateCallback>& UseCallback(
      const scoped_refptr<JsSandboxIsolateCallback>& callback);

  // Java-side JsSandboxIsolate object corresponding to this isolate.
  const base::android::ScopedJavaGlobalRef<jobject> j_isolate_;

  // V8 heap size limit. Must be non-negative.
  //
  // 0 indicates no explicit limit (but use the default V8 limits).
  const size_t isolate_max_heap_size_bytes_;
  // Apart from construction/destruction, must only be used from the isolate
  // thread.
  std::unique_ptr<JsSandboxArrayBufferAllocator> array_buffer_allocator_;

  // Used as a control sequence to add ordering to binder threadpool requests.
  scoped_refptr<base::SequencedTaskRunner> control_task_runner_;
  // Should be used from control_task_runner_.
  bool isolate_init_complete = false;
  // Should be used from control_task_runner_.
  bool destroy_called_before_init = false;
  // Should be used from control_task_runner_.
  std::unique_ptr<base::CancelableTaskTracker> cancelable_task_tracker_;

  // Used for interaction with the isolate.
  scoped_refptr<base::SingleThreadTaskRunner> isolate_task_runner_;
  // Should be used from isolate_task_runner_.
  std::unique_ptr<gin::IsolateHolder> isolate_holder_;
  // Should be used from isolate_task_runner_.
  //
  // This isolate scope is entered during initialization from inside the
  // isolate_task_runner_ thread and exited only at isolate teardown. It is thus
  // used implicitly by all tasks run on the isolate thread.
  std::unique_ptr<v8::Isolate::Scope> isolate_scope_;
  // Should be used from isolate_task_runner_.
  std::unique_ptr<gin::ContextHolder> context_holder_;

  base::Lock named_fd_lock_;
  std::unordered_map<std::string, FdWithLength> named_fd_
      GUARDED_BY(named_fd_lock_);

  // Callbacks associated with evaluations which have begun but not yet
  // finished. More precisely, these are callbacks which have been passed to
  // EvaluateJavascriptOnThread, but which have not yet been called with a final
  // result or error.
  //
  // When a callback is used to send a result or error, it should be removed via
  // UseCallback().
  //
  // This may contain multiple items when evaluations return a promise that must
  // be resolved asynchronously. However, an empty set does not imply that the
  // isolate is idle, as this does not track microtasks or background processing
  // which may result in further execution, such as WASM compilation promises.
  //
  // Used for signaling errors from V8 callbacks.
  std::set<scoped_refptr<JsSandboxIsolateCallback>>
      ongoing_evaluation_callbacks_;

  bool console_enabled_;

  // Inspector objects should be destructed before anything they're inspecting,
  // so they are later in the field list.
  std::unique_ptr<v8_inspector::V8InspectorClient> inspector_client_;
  std::unique_ptr<v8_inspector::V8Inspector> inspector_;
  std::unique_ptr<v8_inspector::V8Inspector::Channel> inspector_channel_;
  std::unique_ptr<v8_inspector::V8InspectorSession> inspector_session_;
};
}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_JS_SANDBOX_SERVICE_JS_SANDBOX_ISOLATE_H_
