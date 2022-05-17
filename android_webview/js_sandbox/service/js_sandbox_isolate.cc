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
#include "gin/try_catch.h"
#include "gin/v8_initializer.h"
#include "js_sandbox_isolate.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;

namespace {
// TODO(crbug.com/1297672): This is what shows up as filename in errors. Revisit
// this once error handling is in place.
constexpr base::StringPiece resource_name = "<expression>";

v8::Local<v8::String> GetSourceLine(v8::Isolate* isolate,
                                    v8::Local<v8::Message> message) {
  auto maybe = message->GetSourceLine(isolate->GetCurrentContext());
  v8::Local<v8::String> source_line;
  return maybe.ToLocal(&source_line) ? source_line : v8::String::Empty(isolate);
}

// Logic borrowed and kept similar to gin::TryCatch::GetStackTrace()
std::string GetStackTrace(v8::TryCatch& try_catch, v8::Isolate* isolate) {
  if (!try_catch.HasCaught()) {
    return "";
  }

  std::stringstream ss;
  v8::Local<v8::Message> message = try_catch.Message();
  ss << gin::V8ToString(isolate, message->Get()) << std::endl
     << gin::V8ToString(isolate, GetSourceLine(isolate, message)) << std::endl;

  v8::Local<v8::StackTrace> trace = message->GetStackTrace();
  if (trace.IsEmpty())
    return ss.str();

  int len = trace->GetFrameCount();
  for (int i = 0; i < len; ++i) {
    v8::Local<v8::StackFrame> frame = trace->GetFrame(isolate, i);
    ss << gin::V8ToString(isolate, frame->GetScriptName()) << ":"
       << frame->GetLineNumber() << ":" << frame->GetColumn() << ": "
       << gin::V8ToString(isolate, frame->GetFunctionName()) << std::endl;
  }
  return ss.str();
}
}  // namespace

namespace android_webview {

JsSandboxIsolate::JsSandboxIsolate() {
  control_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner({});
  isolate_task_runner_ = base::ThreadPool::CreateSingleThreadTaskRunner(
      {base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN, base::MayBlock()},
      base::SingleThreadTaskRunnerThreadMode::DEDICATED);
  control_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&JsSandboxIsolate::CreateCancelableTaskTracker,
                                base::Unretained(this)));
  isolate_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&JsSandboxIsolate::InitializeIsolateOnThread,
                                base::Unretained(this)));
}

JsSandboxIsolate::~JsSandboxIsolate() {}

// Called from Binder thread.
// This method posts evaluation tasks to the control_task_runner_. The
// control_task_runner_ provides ordering to the requests and manages
// cancelable_task_tracker_ which allows us to cancel tasks. The
// control_task_runner_ in turn posts tasks via cancelable_task_tracker_ to the
// isolate_task_runner_ which interacts with the isolate and runs the evaluation
// in v8. Only isolate_task_runner_ should be used to interact with the isolate
// for thread-affine v8 APIs. The callback is invoked from the
// isolate_task_runner_.
jboolean JsSandboxIsolate::EvaluateJavascript(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jstring>& jcode,
    const base::android::JavaParamRef<jobject>& j_success_callback,
    const base::android::JavaParamRef<jobject>& j_failure_callback) {
  std::string code = ConvertJavaStringToUTF8(env, jcode);
  control_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&JsSandboxIsolate::PostEvaluationToIsolateThread,
                     base::Unretained(this), std::move(code),
                     base::BindOnce(&base::android::RunStringCallbackAndroid,
                                    base::android::ScopedJavaGlobalRef<jobject>(
                                        j_success_callback)),
                     base::BindOnce(&base::android::RunStringCallbackAndroid,
                                    base::android::ScopedJavaGlobalRef<jobject>(
                                        j_failure_callback))));
  return true;
}

// Called from Binder thread.
void JsSandboxIsolate::DestroyNative(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  control_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&JsSandboxIsolate::DestroyWhenPossible,
                                base::Unretained(this)));
}

// Called from control sequence.
void JsSandboxIsolate::PostEvaluationToIsolateThread(
    const std::string code,
    FinishedCallback success_callback,
    FinishedCallback error_callback) {
  cancelable_task_tracker_->PostTask(
      isolate_task_runner_.get(), FROM_HERE,
      base::BindOnce(&JsSandboxIsolate::EvaluateJavascriptOnThread,
                     base::Unretained(this), std::move(code),
                     std::move(success_callback), std::move(error_callback)));
}

// Called from control sequence.
void JsSandboxIsolate::CreateCancelableTaskTracker() {
  cancelable_task_tracker_ = std::make_unique<base::CancelableTaskTracker>();
}

// Called from control sequence.
void JsSandboxIsolate::TerminateAndDestroy() {
  // This will cancel all pending executions.
  cancelable_task_tracker_.reset();
  isolate_holder_->isolate()->TerminateExecution();
  isolate_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&JsSandboxIsolate::DeleteSelf, base::Unretained(this)));
}

// Called from control sequence.
void JsSandboxIsolate::DestroyWhenPossible() {
  if (isolate_init_complete) {
    TerminateAndDestroy();
  } else {
    destroy_called_before_init = true;
  }
}

// Called from control sequence.
void JsSandboxIsolate::NotifyInitComplete() {
  if (destroy_called_before_init) {
    TerminateAndDestroy();
  }
  isolate_init_complete = true;
}

// Called from isolate thread.
void JsSandboxIsolate::DeleteSelf() {
  delete this;
}

// Called from isolate thread.
void JsSandboxIsolate::InitializeIsolateOnThread() {
  isolate_holder_ = std::make_unique<gin::IsolateHolder>(
      base::ThreadTaskRunnerHandle::Get(),
      gin::IsolateHolder::IsolateType::kUtility);
  v8::Isolate* isolate = isolate_holder_->isolate();
  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      v8::Context::New(isolate, NULL, v8::Local<v8::ObjectTemplate>());

  context_holder_ = std::make_unique<gin::ContextHolder>(isolate);
  context_holder_->SetContext(context);

  control_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&JsSandboxIsolate::NotifyInitComplete,
                                base::Unretained(this)));
}

// Called from isolate thread.
void JsSandboxIsolate::EvaluateJavascriptOnThread(
    const std::string code,
    FinishedCallback success_callback,
    FinishedCallback error_callback) {
  v8::Isolate::Scope isolate_scope(isolate_holder_->isolate());
  v8::HandleScope handle_scope(isolate_holder_->isolate());
  v8::Context::Scope scope(context_holder_->context());
  v8::Isolate* v8_isolate = isolate_holder_->isolate();
  v8::TryCatch try_catch(v8_isolate);

  // Compile
  v8::ScriptOrigin origin(v8_isolate,
                          gin::StringToV8(v8_isolate, resource_name));
  v8::MaybeLocal<v8::Script> maybe_script = v8::Script::Compile(
      context_holder_->context(), gin::StringToV8(v8_isolate, code), &origin);
  std::string compile_error = "";
  if (try_catch.HasCaught()) {
    compile_error = GetStackTrace(try_catch, v8_isolate);
  }
  v8::Local<v8::Script> script;
  if (!maybe_script.ToLocal(&script)) {
    std::move(error_callback).Run(compile_error);
    return;
  }

  // Run
  v8::Isolate::SafeForTerminationScope safe_for_termination(v8_isolate);
  v8::MaybeLocal<v8::Value> maybe_result =
      script->Run(context_holder_->context());
  std::string run_error = "";
  if (try_catch.HasTerminated()) {
    // Client side will take care of it for now.
    return;
  } else if (try_catch.HasCaught()) {
    run_error = GetStackTrace(try_catch, v8_isolate);
  }
  v8::Local<v8::Value> value;
  if (maybe_result.ToLocal(&value)) {
    std::string result = gin::V8ToString(v8_isolate, value);
    std::move(success_callback).Run(result);
  } else {
    std::move(error_callback).Run(run_error);
  }
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
