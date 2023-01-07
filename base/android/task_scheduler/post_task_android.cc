// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/task_scheduler/post_task_android.h"

#include "base/android/task_scheduler/task_runner_android.h"
#include "base/base_jni_headers/PostTask_jni.h"

namespace base {

// static
void PostTaskAndroid::SignalNativeSchedulerReady() {
  Java_PostTask_onNativeSchedulerReady(base::android::AttachCurrentThread());
}

// static
void PostTaskAndroid::SignalNativeSchedulerShutdownForTesting() {
  Java_PostTask_onNativeSchedulerShutdownForTesting(
      base::android::AttachCurrentThread());
}

void JNI_PostTask_PostDelayedTask(
    JNIEnv* env,
    jint priority,
    jboolean may_block,
    jboolean use_thread_pool,
    jbyte extension_id,
    const base::android::JavaParamRef<jbyteArray>& extension_data,
    const base::android::JavaParamRef<jobject>& task,
    jlong delay,
    const base::android::JavaParamRef<jstring>& runnable_class_name) {
  TaskRunnerAndroid::Create(env, static_cast<jint>(TaskRunnerType::BASE),
                            priority, may_block, use_thread_pool, extension_id,
                            extension_data)
      ->PostDelayedTask(env, task, delay, runnable_class_name);
}

}  // namespace base
