// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "chrome/browser/task_manager/android/task_manager_observer_android.h"
#include "chrome/browser/task_manager/internal/android/jni/TaskManagerServiceBridge_jni.h"
#include "chrome/browser/task_manager/task_manager_interface.h"
#include "chrome/browser/task_manager/task_manager_observer.h"
#include "third_party/jni_zero/jni_zero.h"

namespace task_manager {

static jlong JNI_TaskManagerServiceBridge_AddObserver(
    JNIEnv* env,
    const jni_zero::JavaParamRef<jobject>& observer,
    const jint refresh_time_millis,
    const jint resource_flags) {
  TaskManagerObserverAndroid* delegate =
      new TaskManagerObserverAndroid(env, observer, refresh_time_millis,
                                     static_cast<RefreshType>(resource_flags));
  return reinterpret_cast<jlong>(delegate);
}

static void JNI_TaskManagerServiceBridge_RemoveObserver(JNIEnv* env,
                                                        const jlong ptr) {
  TaskManagerObserverAndroid* delegate =
      reinterpret_cast<TaskManagerObserverAndroid*>(ptr);
  delete delegate;
}

static jni_zero::ScopedJavaLocalRef<jstring>
JNI_TaskManagerServiceBridge_GetTitle(JNIEnv* env, TaskId task_id) {
  return base::android::ConvertUTF16ToJavaString(
      env, TaskManagerInterface::GetTaskManager()->GetTitle(task_id));
}

static jlong JNI_TaskManagerServiceBridge_GetMemoryFootprintUsage(
    JNIEnv* env,
    TaskId task_id) {
  return TaskManagerInterface::GetTaskManager()
      ->GetMemoryFootprintUsage(task_id)
      .InBytes();
}

static jdouble JNI_TaskManagerServiceBridge_GetPlatformIndependentCpuUsage(
    JNIEnv* env,
    TaskId task_id) {
  return TaskManagerInterface::GetTaskManager()->GetPlatformIndependentCPUUsage(
      task_id);
}

static jlong JNI_TaskManagerServiceBridge_GetNetworkUsage(JNIEnv* env,
                                                          TaskId task_id) {
  return TaskManagerInterface::GetTaskManager()
      ->GetNetworkUsage(task_id)
      .InBytes();
}

static jlong JNI_TaskManagerServiceBridge_GetProcessId(JNIEnv* env,
                                                       TaskId task_id) {
  return TaskManagerInterface::GetTaskManager()->GetProcessId(task_id);
}

static jni_zero::ScopedJavaLocalRef<jobject>
JNI_TaskManagerServiceBridge_GetGpuMemoryUsage(JNIEnv* env, TaskId task_id) {
  bool has_duplicates;
  jlong bytes = TaskManagerInterface::GetTaskManager()
                    ->GetGpuMemoryUsage(task_id, &has_duplicates)
                    .InBytes();
  return Java_GpuMemoryUsage_Constructor(env, bytes, has_duplicates);
}

static jboolean JNI_TaskManagerServiceBridge_IsTaskKillable(JNIEnv* env,
                                                            TaskId task_id) {
  return TaskManagerInterface::GetTaskManager()->IsTaskKillable(task_id);
}

static void JNI_TaskManagerServiceBridge_KillTask(JNIEnv* env, TaskId task_id) {
  TaskManagerInterface::GetTaskManager()->KillTask(task_id);
}

}  // namespace task_manager

DEFINE_JNI(TaskManagerServiceBridge)
