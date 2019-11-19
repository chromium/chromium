// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/task_scheduler/task_runner_android.h"

#include "base/android/task_scheduler/post_task_android.h"
#include "base/base_jni_headers/TaskRunnerImpl_jni.h"
#include "base/bind.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/time/time.h"

namespace base {

jlong JNI_TaskRunnerImpl_Init(
    JNIEnv* env,
    jint task_runner_type,
    jboolean priority_set_explicitly,
    jint priority,
    jboolean may_block,
    jboolean thread_pool,
    jboolean current_thread,
    jbyte extension_id,
    const base::android::JavaParamRef<jbyteArray>& extension_data) {
  TaskTraits task_traits = PostTaskAndroid::CreateTaskTraits(
      env, priority_set_explicitly, priority, may_block, thread_pool,
      current_thread, extension_id, extension_data);
  scoped_refptr<TaskRunner> task_runner;
  switch (static_cast<TaskRunnerType>(task_runner_type)) {
    case TaskRunnerType::BASE:
      task_runner = CreateTaskRunner(task_traits);
      break;
    case TaskRunnerType::SEQUENCED:
      task_runner = CreateSequencedTaskRunner(task_traits);
      break;
    case TaskRunnerType::SINGLE_THREAD:
      task_runner = CreateSingleThreadTaskRunner(task_traits);
      break;
  }
  return reinterpret_cast<intptr_t>(new TaskRunnerAndroid(task_runner));
}

TaskRunnerAndroid::TaskRunnerAndroid(scoped_refptr<TaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {}

TaskRunnerAndroid::~TaskRunnerAndroid() = default;

void TaskRunnerAndroid::Destroy(JNIEnv* env) {
  // This could happen on any thread.
  delete this;
}

void TaskRunnerAndroid::PostDelayedTask(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& task,
    jlong delay) {
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PostTaskAndroid::RunJavaTask,
                     base::android::ScopedJavaGlobalRef<jobject>(task)),
      TimeDelta::FromMilliseconds(delay));
}

bool TaskRunnerAndroid::BelongsToCurrentThread(JNIEnv* env) {
  return task_runner_->RunsTasksInCurrentSequence();
}

}  // namespace base
