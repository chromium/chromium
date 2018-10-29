// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/task_scheduler/post_task_android.h"
#include "base/android/task_scheduler/task_runner_android.h"
#include "base/task/post_task.h"
#include "jni/SequencedTaskRunnerImpl_jni.h"

namespace base {

jlong JNI_SequencedTaskRunnerImpl_Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jclass>& jcaller,
    jboolean priority_set_explicitly,
    jint priority,
    jboolean may_block,
    jbyte extension_id,
    const base::android::JavaParamRef<jbyteArray>& extension_data) {
  return reinterpret_cast<intptr_t>(new TaskRunnerAndroid(
      CreateSequencedTaskRunnerWithTraits(PostTaskAndroid::CreateTaskTraits(
          env, priority_set_explicitly, priority, may_block, extension_id,
          extension_data))));
}

}  // namespace base
