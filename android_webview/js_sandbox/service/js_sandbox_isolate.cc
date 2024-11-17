// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/js_sandbox/service/js_sandbox_isolate.h"

#include <errno.h>
#include <unistd.h>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <set>
#include <string>
#include <string_view>

#include "android_webview/js_sandbox/service/js_sandbox_array_buffer_allocator.h"
#include "android_webview/js_sandbox/service/js_sandbox_isolate_callback.h"
#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/immediate_crash.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/numerics/safe_math.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/system/sys_info.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "gin/arguments.h"
#include "gin/array_buffer.h"
#include "gin/function_template.h"
#include "gin/public/context_holder.h"
#include "gin/public/isolate_holder.h"
#include "gin/try_catch.h"
#include "gin/v8_initializer.h"
#include "js_sandbox_isolate.h"
#include "v8/include/v8-array-buffer.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-inspector.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-microtask-queue.h"
#include "v8/include/v8-statistics.h"
#include "v8/include/v8-template.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/js_sandbox/js_sandbox_jni_headers/JsSandboxIsolate_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;

namespace {

// TODO(crbug.com/40215244): This is what shows up as filename in errors.
// Revisit this once error handling is in place.
constexpr std::string_view resource_name = "<expression>";
constexpr jlong kUnknownAssetFileDescriptorLength = -1;
constexpr int64_t kDefaultChunkSize = 1 << 16;

size_t GetAllocatePageSize() {
  return gin::V8Platform::Get()->GetPageAllocator()->AllocatePageSize();
}

// AdjustToValidHeapSize will either round the provided heap size up to a valid
// allocation page size or clip the value to the maximum supported heap size.
size_t AdjustToValidHeapSize(const size_t heap_size_bytes) {
  // This value is not necessarily the same as the system's memory page
  // size. https://bugs.chromium.org/p/v8/issues/detail?id=13172#c6
  const size_t page_size = GetAllocatePageSize();
  const size_t max_supported_heap_size =
      size_t{UINT_MAX} / page_size * page_size;

  if (heap_size_bytes < max_supported_heap_size) {
    return (heap_size_bytes + (page_size - 1)) / page_size * page_size;
  } else {
    return max_supported_heap_size;
  }
}

// Reads content of an Fd from current position to EOF
// Returns true iff success
// Returns false on failure and sets errno
bool ReadFdToStringTillEof(int fd, std::string& contents) {
  contents.clear();
  char temp_buffer[kDefaultChunkSize];
  int64_t bytes_read_this_pass;

  while ((bytes_read_this_pass =
              HANDLE_EINTR(read(fd, temp_buffer, kDefaultChunkSize))) > 0) {
    contents.append(temp_buffer, 0, bytes_read_this_pass);
  }

  if (bytes_read_this_pass == -1) {
    contents.clear();
    return false;
  }

  contents.shrink_to_fit();
  return true;
}

// Skip bytes in case lseek fails
// Returns -1 on read failure and sets errno
// Otherwise returns number of bytes read, including cases where eof is reached
// early and less than expected bytes are read.
int64_t ReadBytesFromFdAndDiscard(int fd, int64_t bytes_to_read) {
  int64_t bytes_read_this_pass;
  int64_t bytes_read_so_far = 0;
  char local_contents[kDefaultChunkSize];

  while (bytes_read_so_far < bytes_to_read) {
    bytes_read_this_pass = HANDLE_EINTR(
        read(fd, &local_contents[0],
             std::min(bytes_to_read - bytes_read_so_far, kDefaultChunkSize)));

    if (bytes_read_this_pass == -1) {
      return -1;
    } else if (bytes_read_this_pass == 0) {
      // eof is reached early
      return bytes_read_so_far;
    }
    bytes_read_so_far += bytes_read_this_pass;
  }

  return bytes_read_so_far;
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

jint remapConsoleMessageErrorLevel(const v8::Isolate::MessageErrorLevel level) {
  // Converted level should match the values specified in the
  // org.chromium.android_webview.js_sandbox.common.IJsSandboxIsolateClient AIDL
  // file (in AndroidX).
  //
  // These will probably remain identical to the underlying v8 enums/constants,
  // but are mapped explicitly here to ensure we can maintain compatibility even
  // if there are changes or additions.
  switch (level) {
    case v8::Isolate::MessageErrorLevel::kMessageLog:
      return 1 << 0;
    case v8::Isolate::MessageErrorLevel::kMessageDebug:
      return 1 << 1;
    case v8::Isolate::MessageErrorLevel::kMessageInfo:
      return 1 << 2;
    case v8::Isolate::MessageErrorLevel::kMessageError:
      return 1 << 3;
    case v8::Isolate::MessageErrorLevel::kMessageWarning:
      return 1 << 4;
    case v8::Isolate::MessageErrorLevel::kMessageAll:
      NOTREACHED();
  }
}

// Converts a V8 inspector (UTF-8 or UTF-16) StringView to a jstring.
base::android::ScopedJavaLocalRef<jstring> StringViewToJavaString(
    JNIEnv* const env,
    const v8_inspector::StringView& string_view) {
  if (string_view.is8Bit()) {
    return base::android::ConvertUTF8ToJavaString(
        env, std::string_view(
                 reinterpret_cast<const char*>(string_view.characters8()),
                 string_view.length()));
  } else {
    return base::android::ConvertUTF16ToJavaString(
        env, std::u16string_view(
                 reinterpret_cast<const char16_t*>(string_view.characters16()),
                 string_view.length()));
  }
}

class NoopInspectorChannel final : public v8_inspector::V8Inspector::Channel {
 public:
  ~NoopInspectorChannel() override = default;
  void sendResponse(
      int callId,
      std::unique_ptr<v8_inspector::StringBuffer> message) override {}
  void sendNotification(
      std::unique_ptr<v8_inspector::StringBuffer> message) override {}
  void flushProtocolNotifications() override {}
};

// This must match the values defined in IJsSandboxIsolateInstanceCallback's
// TERMINATED_ constants;
enum class TerminationStatus {
  kUnknownError = 1,
  kSandboxDead = 2,
  kMemoryLimitExceeded = 3,
};

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

// This class must only be constructed, destructed, and used from the isolate
// thread.
class JsSandboxIsolate::InspectorClient final
    : public v8_inspector::V8InspectorClient {
 public:
  explicit InspectorClient(JsSandboxIsolate& isolate) : isolate_(isolate) {}

  ~InspectorClient() override = default;

  void consoleAPIMessage(const int context_group_id,
                         const v8::Isolate::MessageErrorLevel level,
                         const v8_inspector::StringView& message,
                         const v8_inspector::StringView& url,
                         const unsigned int line_number,
                         const unsigned int column_number,
                         v8_inspector::V8StackTrace* const trace) override {
    if (!isolate_->console_enabled_) {
      return;
    }

    JNIEnv* env = base::android::AttachCurrentThread();
    const jint converted_level = remapConsoleMessageErrorLevel(level);
    base::android::ScopedJavaLocalRef<jstring> java_string_message =
        StringViewToJavaString(env, message);
    // url is actually just the source (file/expression) identifier.
    base::android::ScopedJavaLocalRef<jstring> java_string_source =
        StringViewToJavaString(env, url);
    base::android::ScopedJavaLocalRef<jstring> java_string_trace;
    if (trace && !trace->isEmpty()) {
      StringViewToJavaString(env, trace->toString()->string());
    }

    android_webview::Java_JsSandboxIsolate_consoleMessage(
        env, isolate_->j_isolate_, static_cast<jint>(context_group_id),
        converted_level, java_string_message, java_string_source,
        base::saturated_cast<jint>(line_number),
        base::saturated_cast<jint>(column_number), java_string_trace);
  }

  void consoleClear(const int context_group_id) override {
    if (!isolate_->console_enabled_) {
      return;
    }
    JNIEnv* env = base::android::AttachCurrentThread();
    android_webview::Java_JsSandboxIsolate_consoleClear(
        env, isolate_->j_isolate_, static_cast<jint>(context_group_id));
  }

  double currentTimeMS() override {
    // Note: although this is not monotonically increasing time, this reflects
    // the behaviour of Blink code.
    return base::Time::Now().InMillisecondsFSinceUnixEpoch();
  }

 private:
  const raw_ref<JsSandboxIsolate> isolate_;
};

JsSandboxIsolate::JsSandboxIsolate(
    const base::android::JavaParamRef<jobject>& j_isolate,
    const size_t max_heap_size_bytes)
    : j_isolate_(j_isolate),
      isolate_max_heap_size_bytes_(max_heap_size_bytes),
      array_buffer_allocator_(std::make_unique<JsSandboxArrayBufferAllocator>(
          *gin::ArrayBufferAllocator::SharedInstance(),
          max_heap_size_bytes > 0
              ? max_heap_size_bytes
              : JsSandboxArrayBufferAllocator::kUnlimitedBudget,
          // This is a bit of an implementation detail - gin uses the same
          // underlying allocator for pages and array buffers.
          GetAllocatePageSize())),
      control_task_runner_(base::ThreadPool::CreateSequencedTaskRunner({})),
      isolate_task_runner_(base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN, base::MayBlock()},
          base::SingleThreadTaskRunnerThreadMode::DEDICATED)) {
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
          base::android::ScopedJavaGlobalRef<jobject>(j_callback), false);
  control_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&JsSandboxIsolate::PostEvaluationToIsolateThread,
                     base::Unretained(this), std::move(code),
                     std::move(callback)));
  return true;
}

// Called from Binder thread.
// Refer to comment above EvaluateJavascript method. In addition, this method
// checks for streaming failures.
jboolean JsSandboxIsolate::EvaluateJavascriptWithFd(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const jint fd,
    const jlong length,
    const jlong offset,
    const base::android::JavaParamRef<jobject>& j_callback,
    const base::android::JavaParamRef<jobject>& j_pfd) {
  scoped_refptr<JsSandboxIsolateCallback> callback =
      base::MakeRefCounted<JsSandboxIsolateCallback>(
          base::android::ScopedJavaGlobalRef<jobject>(j_callback), true);

  control_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&JsSandboxIsolate::PostFileDescriptorReadToIsolateThread,
                     base::Unretained(this), fd, length, offset,
                     base::android::ScopedJavaGlobalRef<jobject>(j_pfd),
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

// Called from Binder thread.
void JsSandboxIsolate::SetConsoleEnabled(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const jboolean enable) {
  control_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&JsSandboxIsolate::SetConsoleEnabledOnControlThread,
                     base::Unretained(this), enable));
}

// Called from control sequence.
void JsSandboxIsolate::PostEvaluationToIsolateThread(
    std::string code,
    scoped_refptr<JsSandboxIsolateCallback> callback) {
  cancelable_task_tracker_->PostTask(
      isolate_task_runner_.get(), FROM_HERE,
      base::BindOnce(&JsSandboxIsolate::EvaluateJavascriptOnThread,
                     base::Unretained(this), std::move(code),
                     std::move(callback)));
}

// Called from control sequence
void JsSandboxIsolate::PostFileDescriptorReadToIsolateThread(
    int fd,
    int64_t length,
    int64_t offset,
    base::android::ScopedJavaGlobalRef<jobject> pfd,
    scoped_refptr<JsSandboxIsolateCallback> callback) {
  cancelable_task_tracker_->PostTask(
      isolate_task_runner_.get(), FROM_HERE,
      base::BindOnce(&JsSandboxIsolate::ReadFileDescriptorOnThread,
                     base::Unretained(this), fd, length, offset, std::move(pfd),
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
    std::unique_ptr<v8::Global<v8::ArrayBuffer>> array_buffer,
    std::unique_ptr<v8::Global<v8::Promise::Resolver>> resolver) {
  cancelable_task_tracker_->PostTask(
      isolate_task_runner_.get(), FROM_HERE,
      base::BindOnce(
          &JsSandboxIsolate::ConvertPromiseToArrayBufferInIsolateSequence,
          base::Unretained(this), std::move(name), std::move(array_buffer),
          std::move(resolver)));
}

// Called from control sequence.
//
// The array_buffer's API must only be used from the isolate thread.
void JsSandboxIsolate::ConvertPromiseToFailureInControlSequence(
    std::string name,
    std::unique_ptr<v8::Global<v8::ArrayBuffer>> array_buffer,
    std::unique_ptr<v8::Global<v8::Promise::Resolver>> resolver,
    std::string reason) {
  cancelable_task_tracker_->PostTask(
      isolate_task_runner_.get(), FROM_HERE,
      base::BindOnce(
          &JsSandboxIsolate::ConvertPromiseToFailureInIsolateSequence,
          base::Unretained(this), std::move(name), std::move(array_buffer),
          std::move(resolver), std::move(reason)));
}

// Called from Thread pool.
//
// The array_buffer's API must only be used from the isolate thread, but the
// internal data (inner_buffer) may be accessed in whatever thread is currently
// processing the task, so long as array_buffer remains alive.
void JsSandboxIsolate::ConvertPromiseToArrayBufferInThreadPool(
    base::ScopedFD fd,
    ssize_t length,
    std::string name,
    std::unique_ptr<v8::Global<v8::ArrayBuffer>> array_buffer,
    std::unique_ptr<v8::Global<v8::Promise::Resolver>> resolver,
    void* inner_buffer) {
  if (base::ReadFromFD(fd.get(),
                       base::make_span(static_cast<char*>(inner_buffer),
                                       base::checked_cast<size_t>(length)))) {
    control_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &JsSandboxIsolate::ConvertPromiseToArrayBufferInControlSequence,
            base::Unretained(this), std::move(name), std::move(array_buffer),
            std::move(resolver)));
  } else {
    std::string failure_reason = "Reading data failed.";
    control_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &JsSandboxIsolate::ConvertPromiseToFailureInControlSequence,
            base::Unretained(this), std::move(name), std::move(array_buffer),
            std::move(resolver), std::move(failure_reason)));
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
void JsSandboxIsolate::ReadFileDescriptorOnThread(
    int fd,
    int64_t length,
    int64_t offset,
    base::android::ScopedJavaGlobalRef<jobject> pfd,
    scoped_refptr<JsSandboxIsolateCallback> callback) {
  std::string code;

  if (lseek64(fd, offset, SEEK_SET) == -1) {
    if (errno != ESPIPE) {
      ReportFileDescriptorIOError(
          std::move(pfd), std::move(callback),
          base::StrCat({"Could not seek to offset: ", strerror(errno)}));
      return;
    } else {
      // Just read these bytes and discard
      int64_t bytes_read = ReadBytesFromFdAndDiscard(fd, offset);
      if (bytes_read == -1) {
        ReportFileDescriptorIOError(
            std::move(pfd), std::move(callback),
            base::StrCat({"Could not skip to offset: ", strerror(errno)}));
        return;
      } else if (bytes_read != offset) {
        ReportFileDescriptorIOError(
            std::move(pfd), std::move(callback),
            base::StrCat({"Short read, could only read ",
                          base::NumberToString(bytes_read), " bytes"}));
        return;
      }
    }
  }

  if (length >= 0) {
    code.resize(length);
    if (!base::ReadFromFD(fd, code)) {
      ReportFileDescriptorIOError(std::move(pfd), std::move(callback),
                                  "Failed to read data from file descriptor");
      return;
    }
  } else if (length == kUnknownAssetFileDescriptorLength) {
    if (!ReadFdToStringTillEof(fd, code)) {
      ReportFileDescriptorIOError(
          std::move(pfd), std::move(callback),
          base::StrCat({"Failed to read data till EOF: ", strerror(errno)}));
      return;
    }
  }

  JNIEnv* env = base::android::AttachCurrentThread();

  // check for error on the client side irrespective of errorCode
  base::android::ScopedJavaLocalRef<jstring> error =
      android_webview::Java_JsSandboxIsolate_checkStreamingErrorAndClosePfd(
          env, pfd);

  if (error) {
    callback->ReportFileDescriptorIOFailedError(
        base::StrCat({"Failed to read data from file descriptor: ",
                      ConvertJavaStringToUTF8(env, error)}));
    return;
  }

  // no error reported, proceed for evaluation
  EvaluateJavascriptOnThread(std::move(code), std::move(callback));
}

void JsSandboxIsolate::ReportFileDescriptorIOError(
    base::android::ScopedJavaGlobalRef<jobject> pfd,
    scoped_refptr<JsSandboxIsolateCallback> callback,
    std::string errorMessage) {
  JNIEnv* env = base::android::AttachCurrentThread();

  // check for error on the client side irrespective of errorCode
  base::android::ScopedJavaLocalRef<jstring> error =
      android_webview::Java_JsSandboxIsolate_checkStreamingErrorAndClosePfd(
          env, pfd);

  if (error) {
    errorMessage += base::StrCat(
        {"; Application sent error: ", ConvertJavaStringToUTF8(env, error)});
  }

  callback->ReportFileDescriptorIOFailedError(errorMessage);
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
  params->array_buffer_allocator = array_buffer_allocator_.get();
  if (isolate_max_heap_size_bytes_ > 0) {
    params->constraints.ConfigureDefaultsFromHeapSize(
        0, AdjustToValidHeapSize(isolate_max_heap_size_bytes_));
  }
  isolate_holder_ = std::make_unique<gin::IsolateHolder>(
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      gin::IsolateHolder::AccessMode::kSingleThread,
      gin::IsolateHolder::IsolateType::kUtility, std::move(params));
  v8::Isolate* isolate = isolate_holder_->isolate();
  isolate_scope_ = std::make_unique<v8::Isolate::Scope>(isolate);
  isolate->SetMicrotasksPolicy(v8::MicrotasksPolicy::kAuto);

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
    std::string code,
    scoped_refptr<JsSandboxIsolateCallback> callback) {
  ongoing_evaluation_callbacks_.emplace(callback);

  v8::HandleScope handle_scope(isolate_holder_->isolate());
  v8::Context::Scope scope(context_holder_->context());
  v8::Isolate* v8_isolate = isolate_holder_->isolate();
  v8::TryCatch try_catch(v8_isolate);

  // Compile
  v8::ScriptOrigin origin(gin::StringToV8(v8_isolate, resource_name));
  v8::MaybeLocal<v8::Script> maybe_script = v8::Script::Compile(
      context_holder_->context(), gin::StringToV8(v8_isolate, code), &origin);
  std::string compile_error = "";
  if (try_catch.HasCaught()) {
    compile_error = GetStackTrace(try_catch, v8_isolate);
  }
  v8::Local<v8::Script> script;
  if (!maybe_script.ToLocal(&script)) {
    UseCallback(callback)->ReportError(
        JsSandboxIsolateCallback::ErrorType::kJsEvaluationError, compile_error);
    return;
  }

  // Run
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
        UseCallback(callback)->ReportResult(result);
        return;
      }
      if (promise->State() == v8::Promise::PromiseState::kRejected) {
        v8::Local<v8::Message> message = v8::Exception::CreateMessage(
            isolate_holder_->isolate(), promise->Result());
        std::string error_message = GetStackTrace(message, v8_isolate);
        UseCallback(callback)->ReportError(
            JsSandboxIsolateCallback::ErrorType::kJsEvaluationError,
            error_message);
        return;
      }
      v8::Local<v8::Function> fulfill_fun =
          gin::CreateFunctionTemplate(
              v8_isolate,
              base::BindRepeating(&JsSandboxIsolate::PromiseFulfillCallback,
                                  base::Unretained(this),
                                  base::RetainedRef(callback)))
              ->GetFunction(context_holder_->context())
              .ToLocalChecked();
      v8::Local<v8::Function> reject_fun =
          gin::CreateFunctionTemplate(
              v8_isolate,
              base::BindRepeating(&JsSandboxIsolate::PromiseRejectCallback,
                                  base::Unretained(this),
                                  base::RetainedRef(callback)))
              ->GetFunction(context_holder_->context())
              .ToLocalChecked();

      promise->Then(context_holder_->context(), fulfill_fun, reject_fun)
          .ToLocalChecked();
    } else {
      std::string result = gin::V8ToString(v8_isolate, value);
      UseCallback(callback)->ReportResult(result);
    }
  } else {
    UseCallback(callback)->ReportError(
        JsSandboxIsolateCallback::ErrorType::kJsEvaluationError, run_error);
  }
}

void JsSandboxIsolate::PromiseFulfillCallback(
    scoped_refptr<JsSandboxIsolateCallback> callback,
    gin::Arguments* args) {
  std::string result;
  args->GetNext(&result);
  UseCallback(callback)->ReportResult(result);
}

void JsSandboxIsolate::PromiseRejectCallback(
    scoped_refptr<JsSandboxIsolateCallback> callback,
    gin::Arguments* args) {
  v8::HandleScope handle_scope(isolate_holder_->isolate());
  v8::Local<v8::Value> value;
  args->GetNext(&value);
  v8::Local<v8::Message> message =
      v8::Exception::CreateMessage(isolate_holder_->isolate(), value);
  std::string error_message =
      GetStackTrace(message, isolate_holder_->isolate());
  UseCallback(callback)->ReportError(
      JsSandboxIsolateCallback::ErrorType::kJsEvaluationError, error_message);
}

// Called from isolate thread.
void JsSandboxIsolate::ConvertPromiseToArrayBufferInIsolateSequence(
    std::string name,
    std::unique_ptr<v8::Global<v8::ArrayBuffer>> array_buffer,
    std::unique_ptr<v8::Global<v8::Promise::Resolver>> resolver) {
  v8::HandleScope handle_scope(isolate_holder_->isolate());
  v8::Context::Scope scope(context_holder_->context());

  resolver->Get(isolate_holder_->isolate())
      ->Resolve(context_holder_->context(),
                array_buffer->Get(isolate_holder_->isolate()))
      .ToChecked();
}

// Called from isolate thread.
//
// We pass the array_buffer to the isolate thread so that it (or the handle)
// only gets destructed from the isolate thread.
void JsSandboxIsolate::ConvertPromiseToFailureInIsolateSequence(
    std::string name,
    std::unique_ptr<v8::Global<v8::ArrayBuffer>> array_buffer,
    std::unique_ptr<v8::Global<v8::Promise::Resolver>> resolver,
    std::string reason) {
  v8::HandleScope handle_scope(isolate_holder_->isolate());
  v8::Context::Scope scope(context_holder_->context());

  // Allow array buffer to be garbage collectable before further V8 calls.
  array_buffer = nullptr;

  resolver->Get(isolate_holder_->isolate())
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
    if (entry != named_fd_.end()) {
      // When we move the fd, we invalidate the entry in the map such that it
      // cannot be used again, even if the operation fails before we read any
      // data from the pipe.
      fd = std::move(entry->second.fd);
      length = entry->second.length;
    }
  }
  if (!fd.is_valid()) {
    std::string reason = "No NamedData available with the given name";
    global_resolver.Get(isolate_holder_->isolate())
        ->Reject(context_holder_->context(),
                 v8::Exception::Error(
                     gin::StringToV8(isolate_holder_->isolate(), reason)))
        .ToChecked();
    args->Return(promise);
    return;
  }

  // V8 only accounts for the external memory used by backing stores once they
  // are bound to an array buffer. So we set up the whole array buffer up-front
  // on the isolate thread. (This will prevent V8's view of external memory
  // usage getting out of sync with our own.)
  v8::MaybeLocal<v8::ArrayBuffer> maybe_array_buffer =
      tryAllocateArrayBuffer(length);
  if (maybe_array_buffer.IsEmpty()) {
    const std::string reason =
        "Array buffer allocation failed for consumeNamedDataAsArrayBuffer";
    global_resolver.Get(isolate_holder_->isolate())
        ->Reject(context_holder_->context(),
                 v8::Exception::RangeError(
                     gin::StringToV8(isolate_holder_->isolate(), reason)))
        .ToChecked();
    args->Return(promise);
    return;
  }

  v8::Local<v8::ArrayBuffer> local_array_buffer =
      maybe_array_buffer.ToLocalChecked();
  void* const inner_buffer = local_array_buffer->Data();
  // V8 documentation provides no guarantees about the thread-safety of Globals
  // - even move construction/destruction. Wrap it in a unique_ptr so that it
  // can be treated as an opaque pointer until it's handed back to the isolate
  // thread.
  std::unique_ptr<v8::Global<v8::ArrayBuffer>> global_array_buffer(
      std::make_unique<v8::Global<v8::ArrayBuffer>>(
          isolate, std::move(local_array_buffer)));
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&JsSandboxIsolate::ConvertPromiseToArrayBufferInThreadPool,
                     base::Unretained(this), std::move(fd), length,
                     std::move(name), std::move(global_array_buffer),
                     std::make_unique<v8::Global<v8::Promise::Resolver>>(
                         std::move(global_resolver)),
                     inner_buffer));
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
  ReportOutOfMemory();
  FreezeThread();
}

// Called from isolate thread
void JsSandboxIsolate::ReportOutOfMemory() {
  LOG(ERROR) << "Isolate has OOMed";

  const uint64_t memory_limit = uint64_t{isolate_max_heap_size_bytes_};
  v8::HeapStatistics heap_statistics;
  isolate_holder_->isolate()->GetHeapStatistics(&heap_statistics);
  const uint64_t v8_heap_usage = heap_statistics.used_heap_size();
  // Note that we use our own memory accounting, and not V8's external memory
  // accounting, for non-heap usage. These numbers can differ, particularly as
  // our own memory accounting considers whole pages rather than just bytes.
  const uint64_t non_v8_heap_usage =
      uint64_t{array_buffer_allocator_->GetUsage()};

  std::ostringstream details;
  details << "Memory limit exceeded.\n";
  if (memory_limit > 0) {
    details << "Memory limit: " << memory_limit << " bytes\n";
  } else {
    details << "Memory limit not explicitly configured\n";
  }
  details << "V8 heap usage: " << v8_heap_usage << " bytes\n";
  details << "Non-V8 heap usage: " << non_v8_heap_usage << " bytes\n";
  const std::string details_str = details.str();

  JNIEnv* const env = base::android::AttachCurrentThread();

  const bool client_got_termination =
      android_webview::Java_JsSandboxIsolate_sendTermination(
          env, j_isolate_,
          static_cast<jint>(TerminationStatus::kMemoryLimitExceeded),
          base::android::ConvertUTF8ToJavaString(env, details_str));
  if (client_got_termination) {
    // Don't send any evaluation errors - the client will deal with them itself.
    return;
  }

  bool client_notified_via_evaluation = false;
  if (ongoing_evaluation_callbacks_.size() > 0) {
    // It is safe to erase items from a std::set while iterating through it.
    auto callback_it = ongoing_evaluation_callbacks_.begin();
    while (callback_it != ongoing_evaluation_callbacks_.end()) {
      UseCallback(*callback_it)
          ->ReportError(
              JsSandboxIsolateCallback::ErrorType::kMemoryLimitExceeded,
              details_str);
      callback_it++;
    }
    client_notified_via_evaluation = true;
  }

  // Some pre-stable clients do not support termination notifications and only
  // support signaling OOMs via evaluation callbacks. Ensure the client has been
  // notified through at least one mechanism.
  CHECK(client_notified_via_evaluation)
      << "Isolate ran out of memory but the client does not support "
      << "termination notifications and there are no ongoing evaluations "
      << "through which to signal an error.";
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

// Called from isolate thread.
//
// Attempts to allocate an array buffer of given size. If unsuccessful, no array
// is returned, instead of an OOM crash.
//
// The public V8 APIs don't expose native methods for trying to allocate an
// array buffer without the risk of an OOM crash.
//
// The returned buffer will not be resizable.
v8::MaybeLocal<v8::ArrayBuffer> JsSandboxIsolate::tryAllocateArrayBuffer(
    const size_t length) {
  void* buffer = array_buffer_allocator_->Allocate(length);
  if (!buffer) {
    // Encourage V8 to perform some garbage collection, which might result in
    // previous array buffers getting deallocated. Note this won't free memory
    // from the heap itself, but it will clean up any garbage which is keeping
    // otherwise disused array buffers alive.
    //
    // Note that this may cause overly aggressive garbage collection, but is the
    // only sensible API provided.
    isolate_holder_->isolate()->LowMemoryNotification();
    // Try again after GC.
    buffer = array_buffer_allocator_->Allocate(length);
    if (!buffer) {
      return v8::MaybeLocal<v8::ArrayBuffer>();
    }
  }

  std::unique_ptr<v8::BackingStore> backing_store =
      v8::ArrayBuffer::NewBackingStore(
          buffer, length,
          [](void* buffer_to_delete, size_t length, void* allocator) {
            static_cast<v8::ArrayBuffer::Allocator*>(allocator)->Free(
                buffer_to_delete, length);
          },
          array_buffer_allocator_.get());

  // We do not need to call AdjustAmountOfExternalAllocatedMemory ourselves. V8
  // will automatically call AdjustAmountOfExternalAllocatedMemory with the size
  // of the backing store involved, which may further trigger garbage
  // collections if memory usage is being unreasonable. This is done deep within
  // the call to v8::ArrayBuffer::New().
  return v8::MaybeLocal<v8::ArrayBuffer>(v8::ArrayBuffer::New(
      isolate_holder_->isolate(), std::move(backing_store)));
}

// Called from isolate thread.
void JsSandboxIsolate::EnableOrDisableInspectorAsNeeded() {
  const bool needed = console_enabled_;
  const bool already_enabled = bool{inspector_client_};

  if (already_enabled && !needed) {
    inspector_session_.reset();
    inspector_channel_.reset();
    inspector_.reset();
    inspector_client_.reset();
  } else if (!already_enabled && needed) {
    v8::HandleScope handle_scope(isolate_holder_->isolate());
    v8::Context::Scope scope(context_holder_->context());

    constexpr int context_group_id = 1;
    inspector_client_ = std::make_unique<InspectorClient>(*this);
    inspector_ = v8_inspector::V8Inspector::create(isolate_holder_->isolate(),
                                                   inspector_client_.get());
    inspector_channel_ =
        static_cast<std::unique_ptr<v8_inspector::V8Inspector::Channel>>(
            std::make_unique<NoopInspectorChannel>());
    inspector_session_ =
        inspector_->connect(context_group_id, inspector_channel_.get(),
                            /*state=*/v8_inspector::StringView(),
                            v8_inspector::V8Inspector::kFullyTrusted,
                            v8_inspector::V8Inspector::kNotWaitingForDebugger);
    inspector_->contextCreated(v8_inspector::V8ContextInfo(
        context_holder_->context(), context_group_id,
        /*humanReadableName=*/v8_inspector::StringView()));
  }
}

// Called from control sequence.
void JsSandboxIsolate::SetConsoleEnabledOnControlThread(const bool enable) {
  cancelable_task_tracker_->PostTask(
      isolate_task_runner_.get(), FROM_HERE,
      base::BindOnce(&JsSandboxIsolate::SetConsoleEnabledOnIsolateThread,
                     base::Unretained(this), enable));
}

// Called from isolate thread.
void JsSandboxIsolate::SetConsoleEnabledOnIsolateThread(const bool enable) {
  console_enabled_ = enable;
  EnableOrDisableInspectorAsNeeded();
}

const scoped_refptr<JsSandboxIsolateCallback>& JsSandboxIsolate::UseCallback(
    const scoped_refptr<JsSandboxIsolateCallback>& callback) {
  const size_t removed = ongoing_evaluation_callbacks_.erase(callback);
  CHECK_EQ(removed, size_t{1});
  return callback;
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
    const base::android::JavaParamRef<jobject>& j_sandbox_isolate,
    jlong max_heap_size_bytes) {
  CHECK_GE(max_heap_size_bytes, 0);
  JsSandboxIsolate* processor = new JsSandboxIsolate(
      j_sandbox_isolate, base::saturated_cast<size_t>(max_heap_size_bytes));
  return reinterpret_cast<intptr_t>(processor);
}

}  // namespace android_webview
