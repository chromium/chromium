// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/task_scheduler/task_runner_android.h"

#include "base/android/task_scheduler/post_task_android.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "jni/TaskRunnerImpl_jni.h"

namespace base {

jlong JNI_TaskRunnerImpl_Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jclass>& jcaller,
    jboolean priority_set_explicitly,
    jint priority,
    jboolean may_block,
    jbyte extension_id,
    const base::android::JavaParamRef<jbyteArray>& extension_data) {
  return reinterpret_cast<intptr_t>(new TaskRunnerAndroid(
      CreateTaskRunnerWithTraits(PostTaskAndroid::CreateTaskTraits(
          env, priority_set_explicitly, priority, may_block, extension_id,
          extension_data))));
}

TaskRunnerAndroid::TaskRunnerAndroid(scoped_refptr<TaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {}

TaskRunnerAndroid::~TaskRunnerAndroid() = default;

void TaskRunnerAndroid::Finalize(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& caller) {
  // This will happen on the Java finalizer thread.
  delete this;
}

void TaskRunnerAndroid::PostTask(JNIEnv* env,
                                 const base::android::JavaRef<jobject>& caller,
                                 const base::android::JavaRef<jobject>& task) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PostTaskAndroid::RunJavaTask,
                     base::android::ScopedJavaGlobalRef<jobject>(task)));
}

}  // namespace base
