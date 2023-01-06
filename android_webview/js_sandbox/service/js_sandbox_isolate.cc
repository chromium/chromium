// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/js_sandbox/service/js_sandbox_isolate.h"

#include <unistd.h>
#include <algorithm>
#include <memory>
#include <string>

#include "android_webview/js_sandbox/js_sandbox_jni_headers/JsSandboxIsolate_jni.h"
#include "android_webview/js_sandbox/service/js_sandbox_isolate_callback.h"
#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/auto_reset.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/immediate_crash.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_piece.h"
#include "base/synchronization/waitable_event.h"
#include "base/system/sys_info.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/thread_restrictions.h"
#include "gin/arguments.h"
#include "gin/array_buffer.h"
#include "gin/function_template.h"
#include "gin/public/context_holder.h"
#include "gin/public/isolate_holder.h"
#include "gin/try_catch.h"
#include "gin/v8_initializer.h"
#include "js_sandbox_isolate.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-microtask-queue.h"
#include "v8/include/v8-statistics.h"
#include "v8/include/v8-template.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;

namespace {
// TODO(crbug.com/1297672): This is what shows up as filename in errors. Revisit
// this once error handling is in place.
constexpr base::StringPiece resource_name = "<expression>";

// AdjustToValidHeapSize will either round the provided heap size up to a valid
// allocation page size or clip the value to the maximum supported heap size.
size_t AdjustToValidHeapSize(const uint64_t heap_size_bytes) {
  // The value of 64K should just work on all platforms. Smaller page sizes
  // might work in practice, although we currently don't have long-term
  // guarantees. This value is not necessarily the same as the system's memory
  // page size. https://bugs.chromium.org/p/v8/issues/detail?id=13172#c6
  constexpr size_t page_size = 65536;
  constexpr size_t max_supported_heap_size =
      (size_t)UINT_MAX / page_size * page_size;

  if (heap_size_bytes < (uint64_t)max_supported_heap_size) {
    return ((size_t)heap_size_bytes + (page_size - 1)) / page_size * page_size;
  } else {
    return max_supported_heap_size;
  }
}

v8::Local<v8::String> GetSourceLine(v8::Isolate* isolate,
                                    v8::Local<v8::Message> message) {
  auto maybe = message->GetSourceLine(isolate->GetCurrentContext());
  v8::Local<v8::String> source_line;
  return maybe.ToLocal(&source_line) ? source_line : v8::String::Empty(isolate);
}

std::string GetStackTrace(v8::Local<v8::Message>& message,
                          v8::Isolate* isolate) {
  std::stringstream ss;
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

// Logic borrowed and kept similar to gin::TryCatch::GetStackTrace()
std::string GetStackTrace(v8::TryCatch& try_catch, v8::Isolate* isolate) {
  if (!try_catch.HasCaught()) {
    return "";
  }
  v8::Local<v8::Message> message = try_catch.Message();
  return GetStackTrace(message, isolate);
}

void WasmAsyncResolvePromiseCallback(v8::Isolate* isolate,
                                     v8::Local<v8::Context> context,
                                     v8::Local<v8::Promise::Resolver> resolver,
                                     v8::Local<v8::Value> compilation_result,
                                     v8::WasmAsyncSuccess success) {
  v8::MicrotasksScope microtasks_scope(
      isolate, v8::MicrotasksScope::kDoNotRunMicrotasks);
  if (success == v8::WasmAsyncSuccess::kSuccess) {
    CHECK(resolver->Resolve(context, compilation_result).FromJust());
  } else {
    CHECK(resolver->Reject(context, compilation_result).FromJust());
  }
}

}  // namespace

namespace android_webview {

class FdWithLength {
 public:
  base::ScopedFD fd;
  ssize_t length;

  FdWithLength(int fd, ssize_t len);
  ~FdWithLength() = default;
  FdWithLength(FdWithLength&&) = default;
  FdWithLength& operator=(FdWithLength&&) = default;
};

FdWithLength::FdWithLength(int fd_input, ssize_t len) {
  fd = base::ScopedFD(fd_input);
  length = len;
}

JsSandboxIsolate::JsSandboxIsolate(jlong max_heap_size_bytes)
    : isolate_max_heap_size_bytes_(max_heap_size_bytes) {
  CHECK_GE(isolate_max_heap_size_bytes_, 0);
  control_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner({});
  isolate_task_runner_ = base::ThreadPool::CreateSingleThreadTaskRunner(
      {base::TaskPriority::USER_BLOCKING,
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
    const base::android::JavaParamRef<jobject>& j_callback) {
  std::string code = ConvertJavaStringToUTF8(env, jcode);
  scoped_refptr<JsSandboxIsolateCallback> callback =
      base::MakeRefCounted<JsSandboxIsolateCallback>(
          base::android::ScopedJavaGlobalRef(j_callback));
  control_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&JsSandboxIsolate::PostEvaluationToIsolateThread,
                     base::Unretained(this), std::move(code),
                     std::move(callback)));
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

// Called from Binder thread.
jboolean JsSandboxIsolate::ProvideNamedData(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jstring>& jname,
    const jint fd,
    const jint length) {
  std::string name = ConvertJavaStringToUTF8(env, jname);
  base::AutoLock hold(named_fd_lock_);
  FdWithLength fd_with_length(fd, length);
  return named_fd_.insert(std::make_pair(name, std::move(fd_with_length)))
      .second;
}

// Called from control sequence.
void JsSandboxIsolate::PostEvaluationToIsolateThread(
    const std::string code,
    scoped_refptr<JsSandboxIsolateCallback> callback) {
  cancelable_task_tracker_->PostTask(
      isolate_task_runner_.get(), FROM_HERE,
      base::BindOnce(&JsSandboxIsolate::EvaluateJavascriptOnThread,
                     base::Unretained(this), std::move(code),
                     std::move(callback)));
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

// Called from control sequence.
void JsSandboxIsolate::ConvertPromiseToArrayBufferInControlSequence(
    std::string name,
    std::unique_ptr<v8::BackingStore> backing_store) {
  cancelable_task_tracker_->PostTask(
      isolate_task_runner_.get(), FROM_HERE,
      base::BindOnce(
          &JsSandboxIsolate::ConvertPromiseToArrayBufferInIsolateSequence,
          base::Unretained(this), std::move(name), std::move(backing_store)));
}

// Called from control sequence.
void JsSandboxIsolate::ConvertPromiseToFailureInControlSequence(
    std::string name,
    std::string reason) {
  cancelable_task_tracker_->PostTask(
      isolate_task_runner_.get(), FROM_HERE,
      base::BindOnce(
          &JsSandboxIsolate::ConvertPromiseToFailureInIsolateSequence,
          base::Unretained(this), std::move(name), std::move(reason)));
}

// Called from Thread pool.
void JsSandboxIsolate::ConvertPromiseToArrayBufferInThreadPool(
    base::ScopedFD fd,
    ssize_t length,
    std::string name) {
  char* buffer =
      (char*)gin::ArrayBufferAllocator::SharedInstance()->Allocate(length);
  if (base::ReadFromFD(fd.get(), buffer, length)) {
    auto deleter = [](void* buffer, size_t length, void* data) {
      gin::ArrayBufferAllocator::SharedInstance()->Free(buffer, length);
    };

    std::unique_ptr<v8::BackingStore> backing_store =
        v8::ArrayBuffer::NewBackingStore(buffer, length, deleter, nullptr);
    control_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &JsSandboxIsolate::ConvertPromiseToArrayBufferInControlSequence,
            base::Unretained(this), std::move(name), std::move(backing_store)));
  } else {
    gin::ArrayBufferAllocator::SharedInstance()->Free(buffer, length);
    std::string failure_reason = "Reading data failed.";
    control_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &JsSandboxIsolate::ConvertPromiseToFailureInControlSequence,
            base::Unretained(this), std::move(name),
            std::move(failure_reason)));
  }
}

// Called from isolate thread.
v8::Local<v8::ObjectTemplate> JsSandboxIsolate::CreateAndroidNamespaceTemplate(
    v8::Isolate* isolate) {
  v8::Local<v8::ObjectTemplate> android_namespace_template =
      v8::ObjectTemplate::New(isolate);
  v8::Local<v8::ObjectTemplate> consume_template =
      v8::ObjectTemplate::New(isolate);
  consume_template->Set(
      isolate, "consumeNamedDataAsArrayBuffer",
      gin::CreateFunctionTemplate(
          isolate,
          base::BindRepeating(&JsSandboxIsolate::ConsumeNamedDataAsArrayBuffer,
                              base::Unretained(this))));
  android_namespace_template->Set(isolate, "android", consume_template);
  return android_namespace_template;
}

// Called from isolate thread.
//
// Note that this will never be called if the isolate has "crashed" due to OOM
// and frozen its isolate thread.
void JsSandboxIsolate::DeleteSelf() {
  delete this;
}

// Called from isolate thread.
void JsSandboxIsolate::InitializeIsolateOnThread() {
  std::unique_ptr<v8::Isolate::CreateParams> params =
      gin::IsolateHolder::getDefaultIsolateParams();
  if (isolate_max_heap_size_bytes_ > 0) {
    params->constraints.ConfigureDefaultsFromHeapSize(
        0, AdjustToValidHeapSize(isolate_max_heap_size_bytes_));
  }
  isolate_holder_ = std::make_unique<gin::IsolateHolder>(
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      gin::IsolateHolder::AccessMode::kSingleThread,
      gin::IsolateHolder::IsolateType::kUtility, std::move(params));
  v8::Isolate* isolate = isolate_holder_->isolate();
  v8::Isolate::Scope isolate_scope(isolate);
  isolate->SetMicrotasksPolicy(v8::MicrotasksPolicy::kAuto);
  isolate->SetWasmAsyncResolvePromiseCallback(WasmAsyncResolvePromiseCallback);

  isolate->AddNearHeapLimitCallback(&JsSandboxIsolate::NearHeapLimitCallback,
                                    this);
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::ObjectTemplate> android_template =
      CreateAndroidNamespaceTemplate(isolate);
  v8::Local<v8::Context> context =
      v8::Context::New(isolate, nullptr, android_template);

  context_holder_ = std::make_unique<gin::ContextHolder>(isolate);
  context_holder_->SetContext(context);

  control_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&JsSandboxIsolate::NotifyInitComplete,
                                base::Unretained(this)));
}

// Called from isolate thread.
void JsSandboxIsolate::EvaluateJavascriptOnThread(
    const std::string code,
    scoped_refptr<JsSandboxIsolateCallback> callback) {
  base::AutoReset<JsSandboxIsolateCallback*> callback_autoreset(
      &current_callback_, callback.get());

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
    callback->ReportJsEvaluationError(compile_error);
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
    if (value->IsPromise()) {
      v8::Local<v8::Promise> promise = value.As<v8::Promise>();
      // If the promise is already completed, retrieve and handle the result
      // directly.
      if (promise->State() == v8::Promise::PromiseState::kFulfilled) {
        std::string result = gin::V8ToString(v8_isolate, promise->Result());
        callback->ReportResult(result);
        return;
      }
      if (promise->State() == v8::Promise::PromiseState::kRejected) {
        v8::Local<v8::Message> message = v8::Exception::CreateMessage(
            isolate_holder_->isolate(), promise->Result());
        std::string error_message = GetStackTrace(message, v8_isolate);
        callback->ReportJsEvaluationError(error_message);
        return;
      }
      v8::Local<v8::Function> fulfill_fun =
          gin::CreateFunctionTemplate(
              v8_isolate,
              base::BindRepeating(
                  [](scoped_refptr<JsSandboxIsolateCallback> callback,
                     gin::Arguments* args) {
                    std::string output;
                    args->GetNext(&output);
                    callback->ReportResult(output);
                  },
                  callback))
              ->GetFunction(context_holder_->context())
              .ToLocalChecked();
      v8::Local<v8::Function> reject_fun =
          gin::CreateFunctionTemplate(
              v8_isolate,
              base::BindRepeating(&JsSandboxIsolate::PromiseRejectCallback,
                                  base::Unretained(this), callback))
              ->GetFunction(context_holder_->context())
              .ToLocalChecked();

      promise->Then(context_holder_->context(), fulfill_fun, reject_fun)
          .ToLocalChecked();
    } else {
      std::string result = gin::V8ToString(v8_isolate, value);
      callback->ReportResult(result);
    }
  } else {
    callback->ReportJsEvaluationError(run_error);
  }
}

void JsSandboxIsolate::PromiseRejectCallback(
    scoped_refptr<JsSandboxIsolateCallback> callback,
    gin::Arguments* args) {
  v8::Local<v8::Value> value;
  args->GetNext(&value);
  v8::Local<v8::Message> message =
      v8::Exception::CreateMessage(isolate_holder_->isolate(), value);
  std::string error_message =
      GetStackTrace(message, isolate_holder_->isolate());
  callback->ReportJsEvaluationError(error_message);
}

// Called from isolate thread.
void JsSandboxIsolate::ConvertPromiseToArrayBufferInIsolateSequence(
    std::string name,
    std::unique_ptr<v8::BackingStore> backing_store) {
  v8::Isolate::Scope isolate_scope(isolate_holder_->isolate());
  v8::HandleScope handle_scope(isolate_holder_->isolate());
  v8::Context::Scope scope(context_holder_->context());

  v8::Local<v8::ArrayBuffer> array_buffer = v8::ArrayBuffer::New(
      isolate_holder_->isolate(), std::move(backing_store));
  auto it = named_resolver_.find(name);
  it->second.Get(isolate_holder_->isolate())
      ->Resolve(context_holder_->context(), array_buffer)
      .ToChecked();
  named_resolver_.erase(it);
}

// Called from isolate thread.
void JsSandboxIsolate::ConvertPromiseToFailureInIsolateSequence(
    std::string name,
    std::string reason) {
  v8::Isolate::Scope isolate_scope(isolate_holder_->isolate());
  v8::HandleScope handle_scope(isolate_holder_->isolate());
  v8::Context::Scope scope(context_holder_->context());

  named_resolver_[name]
      .Get(isolate_holder_->isolate())
      ->Reject(context_holder_->context(),
               v8::Exception::Error(
                   gin::StringToV8(isolate_holder_->isolate(), reason)))
      .ToChecked();
}

// Called from isolate thread.
void JsSandboxIsolate::ConsumeNamedDataAsArrayBuffer(gin::Arguments* args) {
  v8::Isolate* isolate = args->isolate();
  v8::Global<v8::Promise::Resolver> global_resolver(
      isolate, v8::Promise::Resolver::New(isolate->GetCurrentContext())
                   .ToLocalChecked());
  v8::Local<v8::Promise> promise = global_resolver.Get(isolate)->GetPromise();
  if (args->Length() != 1) {
    std::string reason = "Unexpected number of arguments";
    global_resolver.Get(isolate_holder_->isolate())
        ->Reject(context_holder_->context(),
                 v8::Exception::Error(
                     gin::StringToV8(isolate_holder_->isolate(), reason)))
        .ToChecked();
    args->Return(promise);
    return;
  }
  std::string name;
  args->GetNext(&name);
  base::ScopedFD fd;
  ssize_t length;
  {
    base::AutoLock lock(named_fd_lock_);
    auto entry = named_fd_.find(name);
    if (entry == named_fd_.end()) {
      std::string reason = "No NamedData available with the given name";
      global_resolver.Get(isolate_holder_->isolate())
          ->Reject(context_holder_->context(),
                   v8::Exception::Error(
                       gin::StringToV8(isolate_holder_->isolate(), reason)))
          .ToChecked();
      args->Return(promise);
      return;
    }
    fd = std::move(entry->second.fd);
    length = entry->second.length;
  }
  named_resolver_.insert({name, std::move(global_resolver)});
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&JsSandboxIsolate::ConvertPromiseToArrayBufferInThreadPool,
                     base::Unretained(this), std::move(fd), length,
                     std::move(name)));
  args->Return(promise);
}

// Called from isolate thread.
[[noreturn]] size_t JsSandboxIsolate::NearHeapLimitCallback(
    void* data,
    size_t /*current_heap_limit*/,
    size_t /*initial_heap_limit*/) {
  android_webview::JsSandboxIsolate* js_sandbox_isolate =
      static_cast<android_webview::JsSandboxIsolate*>(data);
  js_sandbox_isolate->MemoryLimitExceeded();
}

// Called from isolate thread.
[[noreturn]] void JsSandboxIsolate::MemoryLimitExceeded() {
  LOG(ERROR) << "Isolate has OOMed";
  // TODO(ashleynewson): An isolate could run out of memory outside of an
  // evaluation when processing asynchronous code. We should add a crash
  // signalling mechanism which doesn't rely on us having a callback for a
  // currently running evaluation.
  CHECK(current_callback_)
      << "Isolate ran out of memory outside of an evaluation.";
  uint64_t memory_limit = static_cast<uint64_t>(isolate_max_heap_size_bytes_);
  v8::HeapStatistics heap_statistics;
  isolate_holder_->isolate()->GetHeapStatistics(&heap_statistics);
  uint64_t heap_usage = heap_statistics.used_heap_size();
  current_callback_->ReportMemoryLimitExceededError(memory_limit, heap_usage);
  FreezeThread();
}

// Halt thread until process dies.
[[noreturn]] void JsSandboxIsolate::FreezeThread() {
  // There is no well-defined way to fully terminate a thread prematurely, so we
  // idle the thread forever.
  //
  // TODO(ashleynewson): In future, we may want to look into ways to cleanup or
  // even properly terminate the thread if language or V8 features allow for it,
  // as we currently hold onto (essentially leaking) all resources this isolate
  // has accumulated up to this point. C++20's <stop_token> (not permitted in
  // Chromium at time of writing) may contribute to such a future solution.

  base::ScopedAllowBaseSyncPrimitives allow_base_sync_primitives;
  base::WaitableEvent().Wait();
  // Unreachable. Make sure the compiler understands that.
  base::ImmediateCrash();
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
    JNIEnv* env,
    jlong max_heap_size_bytes) {
  JsSandboxIsolate* processor = new JsSandboxIsolate(max_heap_size_bytes);
  return reinterpret_cast<intptr_t>(processor);
}

}  // namespace android_webview
