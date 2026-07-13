// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/android/contextual_tasks_control_coordinator_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/check.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after headers that declare FromJniType / ToJniType
#include "chrome/browser/contextual_tasks/jni_headers/ContextualTasksControlCoordinator_jni.h"

namespace contextual_tasks {

static int64_t JNI_ContextualTasksControlCoordinator_Init(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& obj,
    Profile* profile) {
  return reinterpret_cast<intptr_t>(
      new ContextualTasksControlCoordinatorAndroid(env, obj, profile));
}

ContextualTasksControlCoordinatorAndroid::
    ContextualTasksControlCoordinatorAndroid(
        JNIEnv* env,
        const jni_zero::JavaRef<jobject>& obj,
        Profile* profile)
    : java_obj_(env, obj) {
  DCHECK(profile);
  if (auto* service = ContextualTasksServiceFactory::GetForProfile(profile)) {
    scoped_observation_.Observe(service);
  }
}

ContextualTasksControlCoordinatorAndroid::
    ~ContextualTasksControlCoordinatorAndroid() = default;

void ContextualTasksControlCoordinatorAndroid::Destroy(JNIEnv* env) {
  delete this;
}

void ContextualTasksControlCoordinatorAndroid::OnTaskAdded(
    const ContextualTask& task,
    ContextualTasksService::TriggerSource source) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContextualTasksControlCoordinator_onTaskTitleChanged(
      env, java_obj_, task.GetTaskId().AsLowercaseString(), task.GetTitle());
}

void ContextualTasksControlCoordinatorAndroid::OnTaskUpdated(
    const ContextualTask& task,
    ContextualTasksService::TriggerSource source) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContextualTasksControlCoordinator_onTaskTitleChanged(
      env, java_obj_, task.GetTaskId().AsLowercaseString(), task.GetTitle());
}

void ContextualTasksControlCoordinatorAndroid::OnTaskRemoved(
    const base::Uuid& task_id,
    ContextualTasksService::TriggerSource source) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContextualTasksControlCoordinator_onTaskRemoved(
      env, java_obj_, task_id.AsLowercaseString());
}

void ContextualTasksControlCoordinatorAndroid::OnWillBeDestroyed() {
  scoped_observation_.Reset();
}

}  // namespace contextual_tasks

DEFINE_JNI(ContextualTasksControlCoordinator)
