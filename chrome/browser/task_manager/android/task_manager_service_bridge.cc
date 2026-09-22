// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>

#include "base/android/jni_string.h"
#include "base/byte_size.h"
#include "chrome/browser/task_manager/android/task_manager_observer_android.h"
#include "chrome/browser/task_manager/task_manager_interface.h"
#include "chrome/browser/task_manager/task_manager_observer.h"
#include "third_party/jni_zero/jni_zero.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"

// Must come after headers that provide symbols used by @JniType.
#include "chrome/browser/task_manager/internal/android/jni/TaskManagerServiceBridge_jni.h"

namespace task_manager {

static int64_t JNI_TaskManagerServiceBridge_AddObserver(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& observer,
    const int32_t refresh_time_millis,
    const int32_t resource_flags) {
  TaskManagerObserverAndroid* delegate =
      new TaskManagerObserverAndroid(env, observer, refresh_time_millis,
                                     static_cast<RefreshType>(resource_flags));
  return reinterpret_cast<int64_t>(delegate);
}

static void JNI_TaskManagerServiceBridge_RemoveObserver(const int64_t ptr) {
  TaskManagerObserverAndroid* delegate =
      reinterpret_cast<TaskManagerObserverAndroid*>(ptr);
  delete delegate;
}

static std::u16string JNI_TaskManagerServiceBridge_GetTitle(TaskId task_id) {
  return TaskManagerInterface::GetTaskManager()->GetTitle(task_id);
}

static SkBitmap JNI_TaskManagerServiceBridge_GetIcon(TaskId task_id) {
  const gfx::ImageSkia& icon =
      TaskManagerInterface::GetTaskManager()->GetIcon(task_id);
  return icon.GetRepresentation(1.0f).GetBitmap();
}

static int64_t JNI_TaskManagerServiceBridge_GetMemoryFootprintUsage(
    TaskId task_id) {
  std::optional<base::ByteSize> usage =
      TaskManagerInterface::GetTaskManager()->GetMemoryFootprintUsage(task_id);
  return usage ? usage->InBytes() : -1;
}

static double JNI_TaskManagerServiceBridge_GetPlatformIndependentCpuUsage(
    TaskId task_id) {
  return TaskManagerInterface::GetTaskManager()->GetPlatformIndependentCPUUsage(
      task_id);
}

static int64_t JNI_TaskManagerServiceBridge_GetNetworkUsage(TaskId task_id) {
  return TaskManagerInterface::GetTaskManager()
      ->GetNetworkUsage(task_id)
      .InBytes();
}

static int64_t JNI_TaskManagerServiceBridge_GetProcessId(TaskId task_id) {
  return TaskManagerInterface::GetTaskManager()->GetProcessId(task_id);
}

static jni_zero::ScopedJavaLocalRef<jobject>
JNI_TaskManagerServiceBridge_GetGpuMemoryUsage(JNIEnv* env, TaskId task_id) {
  bool has_duplicates;
  std::optional<base::ByteSize> usage =
      TaskManagerInterface::GetTaskManager()->GetGpuMemoryUsage(
          task_id, &has_duplicates);
  int64_t bytes = usage ? usage->InBytes() : -1;
  return Java_GpuMemoryUsage_Constructor(env, bytes, has_duplicates);
}

static bool JNI_TaskManagerServiceBridge_IsTaskKillable(TaskId task_id) {
  return TaskManagerInterface::GetTaskManager()->IsTaskKillable(task_id);
}

static int32_t JNI_TaskManagerServiceBridge_GetType(TaskId task_id) {
  return static_cast<int32_t>(
      TaskManagerInterface::GetTaskManager()->GetType(task_id));
}

static void JNI_TaskManagerServiceBridge_KillTask(TaskId task_id) {
  TaskManagerInterface::GetTaskManager()->KillTask(task_id);
}

}  // namespace task_manager

DEFINE_JNI(TaskManagerServiceBridge)
