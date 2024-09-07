// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/task_scheduler/task_runner_android.h"

#include <array>
#include <string>
#include <utility>

#include "base/android/jni_string.h"
#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/task/current_thread.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_impl.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/time/time.h"
#include "base/trace_event/base_tracing.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/android_runtime_jni_headers/Runnable_jni.h"
#include "base/tasks_jni/TaskRunnerImpl_jni.h"

namespace base {

namespace {

TaskRunnerAndroid::UiThreadTaskRunnerCallback& GetUiThreadTaskRunnerCallback() {
  static base::NoDestructor<TaskRunnerAndroid::UiThreadTaskRunnerCallback>
      callback;
  return *callback;
}

void RunJavaTask(base::android::ScopedJavaGlobalRef<jobject> task,
                 const std::string& runnable_class_name) {
  TRACE_EVENT("toplevel", nullptr, [&](::perfetto::EventContext& ctx) {
    std::string event_name =
        base::StrCat({"JniPostTask: ", runnable_class_name});
    ctx.event()->set_name(event_name.c_str());
  });
  JNIEnv* env = jni_zero::AttachCurrentThread();
  JNI_Runnable::Java_Runnable_run(env, task);
}

}  // namespace

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
    std::string& runnable_class_name) {
  // This could be run on any java thread, so we can't cache |env| in the
  // BindOnce because JNIEnv is thread specific.
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&RunJavaTask,
                     base::android::ScopedJavaGlobalRef<jobject>(task),
                     runnable_class_name),
      Milliseconds(delay));
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
      [[fallthrough]];
    case ::TaskTraits::UI_USER_VISIBLE:
      [[fallthrough]];
    case ::TaskTraits::UI_USER_BLOCKING:
      use_thread_pool = false;
      break;
  }
  scoped_refptr<TaskRunner> task_runner;
  if (use_thread_pool) {
    switch (static_cast<TaskRunnerType>(task_runner_type)) {
      case TaskRunnerType::BASE:
        task_runner = base::ThreadPool::CreateTaskRunner(task_traits);
        break;
      case TaskRunnerType::SEQUENCED:
        task_runner = base::ThreadPool::CreateSequencedTaskRunner(task_traits);
        break;
      case TaskRunnerType::SINGLE_THREAD:
        task_runner = base::ThreadPool::CreateSingleThreadTaskRunner(
            task_traits, SingleThreadTaskRunnerThreadMode::SHARED);
        break;
    }
  } else {
    CHECK(static_cast<TaskRunnerType>(task_runner_type) ==
          TaskRunnerType::SINGLE_THREAD);
    CHECK(GetUiThreadTaskRunnerCallback());
    task_runner = GetUiThreadTaskRunnerCallback().Run(
        static_cast<::TaskTraits>(j_task_traits));
  }
  return std::make_unique<TaskRunnerAndroid>(
      task_runner, static_cast<TaskRunnerType>(task_runner_type));
}

// static
void TaskRunnerAndroid::SetUiThreadTaskRunnerCallback(
    UiThreadTaskRunnerCallback callback) {
  GetUiThreadTaskRunnerCallback() = std::move(callback);
}

}  // namespace base
