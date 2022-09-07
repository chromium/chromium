// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_JS_SANDBOX_SERVICE_JS_SANDBOX_ISOLATE_H_
#define ANDROID_WEBVIEW_JS_SANDBOX_SERVICE_JS_SANDBOX_ISOLATE_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/android/scoped_java_ref.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/files/scoped_file.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
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
class BackingStore;
class ObjectTemplate;
}  // namespace v8

namespace android_webview {

class FdWithLength;

class JsSandboxIsolate {
 public:
  explicit JsSandboxIsolate(jlong max_heap_size_bytes = 0);
  ~JsSandboxIsolate();

  using FinishedCallback = base::OnceCallback<void(const std::string&)>;

  jboolean EvaluateJavascript(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& jcode,
      const base::android::JavaParamRef<jobject>& j_success_callback,
      const base::android::JavaParamRef<jobject>& j_failure_callback);
  void DestroyNative(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj);
  jboolean ProvideNamedData(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& obj,
                            const base::android::JavaParamRef<jstring>& jname,
                            const jint fd,
                            const jint length);

 private:
  void DeleteSelf();
  void InitializeIsolateOnThread();
  void EvaluateJavascriptOnThread(const std::string code,
                                  FinishedCallback success_callback,
                                  FinishedCallback failure_callback);
  void PromiseRejectCallback(FinishedCallback error_callback,
                             gin::Arguments* args);

  void TerminateAndDestroy();
  void DestroyWhenPossible();
  void NotifyInitComplete();
  void CreateCancelableTaskTracker();
  void PostEvaluationToIsolateThread(const std::string code,
                                     FinishedCallback success_callback,
                                     FinishedCallback error_callback);
  void ConvertPromiseToArrayBufferInThreadPool(base::ScopedFD fd,
                                               ssize_t length,
                                               std::string name);
  void ConvertPromiseToArrayBufferInControlSequence(
      std::string name,
      std::unique_ptr<v8::BackingStore> backing_store);
  void ConvertPromiseToFailureInControlSequence(std::string name,
                                                std::string reason);
  void ConvertPromiseToFailureInIsolateSequence(std::string name,
                                                std::string reason);
  void ConvertPromiseToArrayBufferInIsolateSequence(
      std::string name,
      std::unique_ptr<v8::BackingStore>);

  void ConsumeNamedDataAsArrayBuffer(gin::Arguments* args);
  v8::Local<v8::ObjectTemplate> CreateAndroidNamespaceTemplate(
      v8::Isolate* isolate);

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
  std::unique_ptr<gin::ContextHolder> context_holder_;
  // Should be used from isolate_task_runner_.
  std::unordered_map<std::string, v8::Global<v8::Promise::Resolver>>
      named_resolver_;

  base::Lock named_fd_lock_;
  std::unordered_map<std::string, FdWithLength> named_fd_
      GUARDED_BY(named_fd_lock_);
  jlong isolate_max_heap_size_bytes_;
};
}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_JS_SANDBOX_SERVICE_JS_SANDBOX_ISOLATE_H_
