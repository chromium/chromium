// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/js_sandbox/service/js_sandbox_isolate.h"

#include <algorithm>
#include <memory>
#include <string>

#include "android_webview/js_sandbox/js_sandbox_jni_headers/JsSandboxIsolate_jni.h"
#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "gin/array_buffer.h"
#include "gin/public/isolate_holder.h"
#include "gin/shell_runner.h"
#include "gin/try_catch.h"
#include "gin/v8_initializer.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;

namespace {
// TODO(crbug.com/1297672): This is what shows up as filename in errors. Revisit
// this once error handling is in place.
constexpr base::StringPiece resource_name = "<expression>";

class SandboxRunnerDelegate : public gin::ShellRunnerDelegate {
 public:
  SandboxRunnerDelegate() {}
  ~SandboxRunnerDelegate() override = default;

  using FinishedCallback = base::OnceCallback<void(const std::string&)>;

  void SetErrorCallback(FinishedCallback error_callback) {
    error_callback_ = std::move(error_callback);
  }

  void UnhandledException(gin::ShellRunner* runner,
                          gin::TryCatch& try_catch) override {
    std::move(error_callback_).Run(try_catch.GetStackTrace());
  }

 private:
  FinishedCallback error_callback_;
};
}  // namespace

namespace android_webview {

JsSandboxIsolate::JsSandboxIsolate() {
  task_runner_ = base::ThreadPool::CreateSingleThreadTaskRunner(
      {base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN, base::MayBlock()},
      base::SingleThreadTaskRunnerThreadMode::DEDICATED);
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&JsSandboxIsolate::InitializeIsolateOnThread,
                                base::Unretained(this)));
}

JsSandboxIsolate::~JsSandboxIsolate() {}

void JsSandboxIsolate::DeleteSelf() {
  delete this;
}

void JsSandboxIsolate::DestroyNative(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  // TODO(crbug.com/1297672): Currently this only posts the deletion task to
  // the task runner which ensures that the deletion happens after all the
  // existing tasks are processed. We may also want to cancel any
  // not-yet-started tasks rather than let them all run. And, ultimately,
  // we'll want to forcibly abort execution in the V8 isolate.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&JsSandboxIsolate::DeleteSelf, base::Unretained(this)));
}

void JsSandboxIsolate::InitializeIsolateOnThread() {
  isolate_holder_ = std::make_unique<gin::IsolateHolder>(
      base::ThreadTaskRunnerHandle::Get(),
      gin::IsolateHolder::IsolateType::kUtility);
  v8::Isolate* isolate = isolate_holder_->isolate();
  delegate_ = std::make_unique<SandboxRunnerDelegate>();
  runner_ = std::make_unique<gin::ShellRunner>(delegate_.get(), isolate);
}

void JsSandboxIsolate::EvaluateJavascriptOnThread(
    const std::string code,
    FinishedCallback success_callback,
    FinishedCallback error_callback) {
  delegate_->SetErrorCallback(std::move(error_callback));
  gin::Runner::Scope scope(runner_.get());
  std::string resource_string(resource_name.begin(), resource_name.end());
  v8::MaybeLocal<v8::Value> maybe = runner_->Run(code, resource_string);
  v8::Local<v8::Value> value;
  if (maybe.ToLocal(&value)) {
    std::string result =
        gin::V8ToString(runner_->GetContextHolder()->isolate(), value);
    std::move(success_callback).Run(result);
  }
}

// A single thread is used to interact with the isolate and post tasks. This
// is because an isolate is not thread safe. Incoming IPCs come from
// different threads belonging to the Binder threadpool. We push all of these
// requests into the isolate thread pool queue and return immediately. Once the
// result is computed, the isolate thread calls the callback.
jboolean JsSandboxIsolate::EvaluateJavascript(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jstring>& jcode,
    const base::android::JavaParamRef<jobject>& j_success_callback,
    const base::android::JavaParamRef<jobject>& j_failure_callback) {
  std::string code = ConvertJavaStringToUTF8(env, jcode);
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&JsSandboxIsolate::EvaluateJavascriptOnThread,
                     base::Unretained(this), std::move(code),
                     base::BindOnce(&base::android::RunStringCallbackAndroid,
                                    base::android::ScopedJavaGlobalRef<jobject>(
                                        j_success_callback)),
                     base::BindOnce(&base::android::RunStringCallbackAndroid,
                                    base::android::ScopedJavaGlobalRef<jobject>(
                                        j_failure_callback))));
  return true;
}

static void JNI_JsSandboxIsolate_InitializeEnvironment(JNIEnv* env) {
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("JsSandboxIsolate");
#ifdef V8_USE_EXTERNAL_STARTUP_DATA
  gin::V8Initializer::LoadV8Snapshot();
#endif
  gin::IsolateHolder::Initialize(gin::IsolateHolder::kStrictMode,
                                 gin::ArrayBufferAllocator::SharedInstance());
}

static jlong JNI_JsSandboxIsolate_CreateNativeJsSandboxIsolateWrapper(
    JNIEnv* env) {
  JsSandboxIsolate* processor = new JsSandboxIsolate();
  return reinterpret_cast<intptr_t>(processor);
}

}  // namespace android_webview
