// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_TASK_SCHEDULER_TASK_RUNNER_ANDROID_H_
#define BASE_ANDROID_TASK_SCHEDULER_TASK_RUNNER_ANDROID_H_

#include <memory>

#include "base/android/jni_weak_ref.h"
#include "base/task/single_thread_task_runner.h"

namespace base {

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.base.task
enum class TaskRunnerType { BASE, SEQUENCED, SINGLE_THREAD };

// Native implementation backing TaskRunnerImpl.java which posts java tasks onto
// a C++ TaskRunner.
class TaskRunnerAndroid {
 public:
  explicit TaskRunnerAndroid(scoped_refptr<TaskRunner> task_runner,
                             TaskRunnerType type);

  TaskRunnerAndroid(const TaskRunnerAndroid&) = delete;
  TaskRunnerAndroid& operator=(const TaskRunnerAndroid&) = delete;

  ~TaskRunnerAndroid();

  void Destroy(JNIEnv* env);

  void PostDelayedTask(JNIEnv* env,
                       const base::android::JavaRef<jobject>& task,
                       jlong delay,
                       jstring runnable_class_name);

  bool BelongsToCurrentThread(JNIEnv* env);

  static std::unique_ptr<TaskRunnerAndroid> Create(
      JNIEnv* env,
      jint task_runner_type,
      jint priority,
      jboolean may_block,
      jboolean use_thread_pool,
      jbyte extension_id,
      const base::android::JavaParamRef<jbyteArray>& extension_data);

 private:
  const scoped_refptr<TaskRunner> task_runner_;
  const TaskRunnerType type_;
};

}  // namespace base

#endif  // BASE_ANDROID_TASK_SCHEDULER_TASK_RUNNER_ANDROID_H_
