// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "chrome/browser/task_manager/android/task_manager_observer_android.h"
#include "chrome/browser/task_manager/internal/android/jni/TaskManagerServiceBridge_jni.h"
#include "chrome/browser/task_manager/task_manager_interface.h"
#include "chrome/browser/task_manager/task_manager_observer.h"
#include "third_party/jni_zero/jni_zero.h"

static jlong JNI_TaskManagerServiceBridge_AddObserver(
    JNIEnv* env,
    const jni_zero::JavaParamRef<jobject>& observer,
    const jint refresh_time_millis,
    const jint resource_flags) {
  task_manager::TaskManagerObserverAndroid* delegate =
      new task_manager::TaskManagerObserverAndroid(
          env, observer, refresh_time_millis,
          static_cast<task_manager::RefreshType>(resource_flags));
  return reinterpret_cast<jlong>(delegate);
}

static void JNI_TaskManagerServiceBridge_RemoveObserver(JNIEnv* env,
                                                        const jlong ptr) {
  task_manager::TaskManagerObserverAndroid* delegate =
      reinterpret_cast<task_manager::TaskManagerObserverAndroid*>(ptr);
  delete delegate;
}

static jni_zero::ScopedJavaLocalRef<jstring>
JNI_TaskManagerServiceBridge_GetTitle(JNIEnv* env,
                                      task_manager::TaskId task_id) {
  return base::android::ConvertUTF16ToJavaString(
      env,
      task_manager::TaskManagerInterface::GetTaskManager()->GetTitle(task_id));
}

static jlong JNI_TaskManagerServiceBridge_GetMemoryFootprintUsage(
    JNIEnv* env,
    task_manager::TaskId task_id) {
  return task_manager::TaskManagerInterface::GetTaskManager()
      ->GetMemoryFootprintUsage(task_id);
}

static jdouble JNI_TaskManagerServiceBridge_GetPlatformIndependentCpuUsage(
    JNIEnv* env,
    task_manager::TaskId task_id) {
  return task_manager::TaskManagerInterface::GetTaskManager()
      ->GetPlatformIndependentCPUUsage(task_id);
}

static jlong JNI_TaskManagerServiceBridge_GetProcessId(
    JNIEnv* env,
    task_manager::TaskId task_id) {
  return task_manager::TaskManagerInterface::GetTaskManager()->GetProcessId(
      task_id);
}

static jboolean JNI_TaskManagerServiceBridge_IsTaskKillable(
    JNIEnv* env,
    task_manager::TaskId task_id) {
  return task_manager::TaskManagerInterface::GetTaskManager()->IsTaskKillable(
      task_id);
}

static void JNI_TaskManagerServiceBridge_KillTask(
    JNIEnv* env,
    task_manager::TaskId task_id) {
  task_manager::TaskManagerInterface::GetTaskManager()->KillTask(task_id);
}
