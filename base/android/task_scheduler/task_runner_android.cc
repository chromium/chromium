// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/task_scheduler/task_runner_android.h"

#include <array>
#include <string>
#include <utility>

#include "base/android/jni_string.h"
#include "base/android/task_scheduler/task_traits_android.h"
#include "base/android_runtime_jni_headers/Runnable_jni.h"
#include "base/base_jni_headers/TaskRunnerImpl_jni.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/task/task_executor.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool/thread_pool_impl.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/time/time.h"
#include "base/trace_event/base_tracing.h"

namespace base {

namespace {

static TaskTraitsExtensionStorage g_ui_thread_extension;

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

// As a class so it can be friend'ed.
class AndroidTaskTraits {
 public:
  AndroidTaskTraits() = delete;

  static TaskTraits CreateForUi(base::TaskPriority priority) {
    return TaskTraits(priority, false, g_ui_thread_extension);
  }
};

jlong JNI_TaskRunnerImpl_Init(JNIEnv* env,
                              jint task_runner_type,
                              jint task_traits) {
  TaskRunnerAndroid* task_runner =
      TaskRunnerAndroid::Create(task_runner_type, task_traits).release();
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
    jint task_runner_type,
    jint j_task_traits) {
  TaskTraits task_traits;
  bool use_thread_pool = true;
  switch (j_task_traits) {
    case ::TaskTraits::BEST_EFFORT:
      task_traits = {TaskPriority::BEST_EFFORT};
      break;
    case ::TaskTraits::BEST_EFFORT_MAY_BLOCK:
      task_traits = {base::MayBlock(), TaskPriority::BEST_EFFORT};
      break;
    case ::TaskTraits::USER_VISIBLE:
      task_traits = {TaskPriority::USER_VISIBLE};
      break;
    case ::TaskTraits::USER_VISIBLE_MAY_BLOCK:
      task_traits = {base::MayBlock(), TaskPriority::USER_VISIBLE};
      break;
    case ::TaskTraits::USER_BLOCKING:
      task_traits = {TaskPriority::USER_BLOCKING};
      break;
    case ::TaskTraits::USER_BLOCKING_MAY_BLOCK:
      task_traits = {base::MayBlock(), TaskPriority::USER_BLOCKING};
      break;
    case ::TaskTraits::UI_BEST_EFFORT:
      use_thread_pool = false;
      task_traits = AndroidTaskTraits::CreateForUi(TaskPriority::BEST_EFFORT);
      break;
    case ::TaskTraits::UI_USER_VISIBLE:
      use_thread_pool = false;
      task_traits = AndroidTaskTraits::CreateForUi(TaskPriority::USER_VISIBLE);
      break;
    case ::TaskTraits::UI_USER_BLOCKING:
      use_thread_pool = false;
      task_traits = AndroidTaskTraits::CreateForUi(TaskPriority::USER_BLOCKING);
      break;
  }
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

// static
void TaskRunnerAndroid::SetUiThreadExtension(
    TaskTraitsExtensionStorage extension) {
  g_ui_thread_extension = extension;
}

}  // namespace base
