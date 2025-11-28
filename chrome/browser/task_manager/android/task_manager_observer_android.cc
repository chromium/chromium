// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/android/task_manager_observer_android.h"

#include "base/android/jni_array.h"
#include "base/android/scoped_java_ref.h"
#include "base/time/time.h"
#include "chrome/browser/task_manager/internal/android/jni/TaskManagerObserver_jni.h"
#include "chrome/browser/task_manager/task_manager_interface.h"
#include "chrome/browser/task_manager/task_manager_observer.h"

namespace task_manager {

TaskManagerObserverAndroid::TaskManagerObserverAndroid(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& obj,
    int refresh_time_millis,
    task_manager::RefreshType resources_flags)
    : TaskManagerObserver(base::Milliseconds(refresh_time_millis),
                          resources_flags),
      java_object_(env, obj) {
  TaskManagerInterface::GetTaskManager()->AddObserver(this);
}

TaskManagerObserverAndroid::~TaskManagerObserverAndroid() {
  TaskManagerInterface::GetTaskManager()->RemoveObserver(this);
}

void TaskManagerObserverAndroid::OnTaskAdded(TaskId id) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_TaskManagerObserver_onTaskAdded(env, java_object_, id);
}

void TaskManagerObserverAndroid::OnTaskToBeRemoved(TaskId id) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_TaskManagerObserver_onTaskToBeRemoved(env, java_object_, id);
}

void TaskManagerObserverAndroid::OnTasksRefreshed(const TaskIdList& task_ids) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_TaskManagerObserver_onTasksRefreshed(
      env, java_object_, base::android::ToJavaLongArray(env, task_ids));
}

void TaskManagerObserverAndroid::OnTasksRefreshedWithBackgroundCalculations(
    const TaskIdList& task_ids) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_TaskManagerObserver_onTasksRefreshedWithBackgroundCalculations(
      env, java_object_, base::android::ToJavaLongArray(env, task_ids));
}

void TaskManagerObserverAndroid::OnTaskUnresponsive(TaskId id) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_TaskManagerObserver_onTaskUnresponsive(env, java_object_, id);
}

}  // namespace task_manager

DEFINE_JNI(TaskManagerObserver)
