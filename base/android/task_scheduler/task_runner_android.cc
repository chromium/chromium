// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/task_scheduler/task_runner_android.h"

#include <array>
#include <string>
#include <utility>

#include "base/android/jni_string.h"
#include "base/android_runtime_jni_headers/Runnable_jni.h"
#include "base/base_jni_headers/TaskRunnerImpl_jni.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/strings/strcat.h"
#include "base/task/task_executor.h"
#include "base/task/thread_pool/thread_pool_impl.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/time/time.h"
#include "base/trace_event/base_tracing.h"

namespace base {

// As a class so it can be friend'ed.
class AndroidTaskTraits {
 public:
  AndroidTaskTraits() = delete;

  static TaskTraits Create(
      JNIEnv* env,
      jint priority,
      jboolean may_block,
      jbyte extension_id,
      const base::android::JavaParamRef<jbyteArray>& extension_data) {
    return TaskTraits(
        static_cast<TaskPriority>(priority), may_block,
        TaskTraitsExtensionStorage(static_cast<uint8_t>(extension_id),
                                   GetExtensionData(env, extension_data)));
  }

 private:
  static std::array<uint8_t, TaskTraitsExtensionStorage::kStorageSize>
  GetExtensionData(
      JNIEnv* env,
      const base::android::JavaParamRef<jbyteArray>& array_object) {
    if (env->IsSameObject(array_object, nullptr))
      return std::array<uint8_t, TaskTraitsExtensionStorage::kStorageSize>();

    jbyteArray array = static_cast<jbyteArray>(array_object);
    DCHECK_EQ(env->GetArrayLength(array),
              static_cast<jsize>(TaskTraitsExtensionStorage::kStorageSize));

    std::array<uint8_t, TaskTraitsExtensionStorage::kStorageSize> result;
    jbyte* src_bytes = env->GetByteArrayElements(array, nullptr);
    memcpy(&result[0], src_bytes, TaskTraitsExtensionStorage::kStorageSize);
    env->ReleaseByteArrayElements(array, src_bytes, JNI_ABORT);
    return result;
  }
};

namespace {

// TODO(1026641): Make destination explicit (separate APIs) on Java side too and
// get rid of the need for TaskTraitsExtension/etc to reach the UI thread.
TaskExecutor* GetTaskExecutor(bool use_thread_pool, const TaskTraits& traits) {
  const bool has_extension =
      traits.extension_id() != TaskTraitsExtensionStorage::kInvalidExtensionId;
  DCHECK(has_extension ^ use_thread_pool)
      << "A destination (e.g. ThreadPool or UiThreadTaskTraits) is required.";

  if (use_thread_pool) {
    DCHECK(ThreadPoolInstance::Get())
        << "Hint: if this is in a unit test, you're likely merely missing a "
           "base::test::TaskEnvironment member in your fixture (or your "
           "fixture is using a base::test::SingleThreadTaskEnvironment and now "
           "needs a full base::test::TaskEnvironment).\n";
    return static_cast<internal::ThreadPoolImpl*>(ThreadPoolInstance::Get());
  }

  // Assume |has_extension| per above invariant.
  TaskExecutor* executor = GetRegisteredTaskExecutorForTraits(traits);
  DCHECK(executor)
      << "A TaskExecutor wasn't yet registered for this extension.\n"
         "Hint: if this is in a unit test, you're likely missing a "
         "content::BrowserTaskEnvironment member in your fixture.";
  return executor;
}

void RunJavaTask(base::android::ScopedJavaGlobalRef<jobject> task,
                 const std::string& runnable_class_name) {
  // JNIEnv is thread specific, but we don't know which thread we'll be run on
  // so we must look it up.
  std::string event_name = base::StrCat({"JniPostTask: ", runnable_class_name});
  TRACE_EVENT("toplevel", nullptr, [&](::perfetto::EventContext& ctx) {
    ctx.event()->set_name(event_name.c_str());
  });
  JNI_Runnable::Java_Runnable_run(base::android::AttachCurrentThread(), task);
}

}  // namespace

jlong JNI_TaskRunnerImpl_Init(
    JNIEnv* env,
    jint task_runner_type,
    jint priority,
    jboolean may_block,
    jboolean use_thread_pool,
    jbyte extension_id,
    const base::android::JavaParamRef<jbyteArray>& extension_data) {
  TaskRunnerAndroid* task_runner =
      TaskRunnerAndroid::Create(env, task_runner_type, priority, may_block,
                                use_thread_pool, extension_id, extension_data)
          .release();
  return reinterpret_cast<intptr_t>(task_runner);
}

TaskRunnerAndroid::TaskRunnerAndroid(scoped_refptr<TaskRunner> task_runner,
                                     TaskRunnerType type)
    : task_runner_(std::move(task_runner)), type_(type) {}

TaskRunnerAndroid::~TaskRunnerAndroid() = default;

void TaskRunnerAndroid::Destroy(JNIEnv* env) {
  // This could happen on any thread.
  delete this;
}

void TaskRunnerAndroid::PostDelayedTask(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& task,
    jlong delay,
    jstring runnable_class_name) {
  // This could be run on any java thread, so we can't cache |env| in the
  // BindOnce because JNIEnv is thread specific.
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &RunJavaTask, base::android::ScopedJavaGlobalRef<jobject>(task),
          android::ConvertJavaStringToUTF8(env, runnable_class_name)),
      Milliseconds(delay));
}

bool TaskRunnerAndroid::BelongsToCurrentThread(JNIEnv* env) {
  // TODO(crbug.com/1026641): Move BelongsToCurrentThread from TaskRunnerImpl to
  // SequencedTaskRunnerImpl on the Java side too.
  if (type_ == TaskRunnerType::BASE)
    return false;
  return static_cast<SequencedTaskRunner*>(task_runner_.get())
      ->RunsTasksInCurrentSequence();
}

// static
std::unique_ptr<TaskRunnerAndroid> TaskRunnerAndroid::Create(
    JNIEnv* env,
    jint task_runner_type,
    jint priority,
    jboolean may_block,
    jboolean use_thread_pool,
    jbyte extension_id,
    const base::android::JavaParamRef<jbyteArray>& extension_data) {
  const TaskTraits task_traits = AndroidTaskTraits::Create(
      env, priority, may_block, extension_id, extension_data);
  TaskExecutor* const task_executor =
      GetTaskExecutor(use_thread_pool, task_traits);
  scoped_refptr<TaskRunner> task_runner;
  switch (static_cast<TaskRunnerType>(task_runner_type)) {
    case TaskRunnerType::BASE:
      task_runner = task_executor->CreateTaskRunner(task_traits);
      break;
    case TaskRunnerType::SEQUENCED:
      task_runner = task_executor->CreateSequencedTaskRunner(task_traits);
      break;
    case TaskRunnerType::SINGLE_THREAD:
      task_runner = task_executor->CreateSingleThreadTaskRunner(
          task_traits, SingleThreadTaskRunnerThreadMode::SHARED);
      break;
  }
  return std::make_unique<TaskRunnerAndroid>(
      task_runner, static_cast<TaskRunnerType>(task_runner_type));
}

}  // namespace base
