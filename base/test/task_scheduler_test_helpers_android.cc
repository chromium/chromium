// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/task_scheduler/task_scheduler.h"
#include "jni/TaskSchedulerTestHelpers_jni.h"

namespace base {

// TaskSchedulerTestHelpers is a friend of TaskScheduler which grants access to
// SetExecutionFenceEnabled.
class TaskSchedulerTestHelpers {
 public:
  // Enables/disables an execution fence that prevents tasks from running.
  static void SetTaskSchedulerExecutionFenceEnabledForTesting(
      bool execution_fence_enabled);
};

// static
void TaskSchedulerTestHelpers::SetTaskSchedulerExecutionFenceEnabledForTesting(
    bool execution_fence_enabled) {
  TaskScheduler::GetInstance()->SetExecutionFenceEnabled(
      execution_fence_enabled);
}

}  // namespace base

void JNI_TaskSchedulerTestHelpers_EnableTaskSchedulerExecutionForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jclass>& jcaller) {
  base::TaskSchedulerTestHelpers::
      SetTaskSchedulerExecutionFenceEnabledForTesting(false);
}

void JNI_TaskSchedulerTestHelpers_DisableTaskSchedulerExecutionForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jclass>& jcaller) {
  base::TaskSchedulerTestHelpers::
      SetTaskSchedulerExecutionFenceEnabledForTesting(true);
}
