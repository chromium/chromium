// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/task_scheduler/post_task_android.h"

#include "base/android/jni_string.h"
#include "base/android_runtime_jni_headers/Runnable_jni.h"
#include "base/base_jni_headers/PostTask_jni.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/trace_event/base_tracing.h"

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

namespace {
std::array<uint8_t, TaskTraitsExtensionStorage::kStorageSize> GetExtensionData(
    JNIEnv* env,
    const base::android::JavaParamRef<jbyteArray>& array_object) {
  if (env->IsSameObject(array_object, nullptr))
    return std::array<uint8_t, TaskTraitsExtensionStorage::kStorageSize>();

  jbyteArray array = static_cast<jbyteArray>(array_object);
  DCHECK_EQ(env->GetArrayLength(array),
            static_cast<jsize>(TaskTraitsExtensionStorage::kStorageSize));

  std::array<uint8_t, TaskTraitsExtensionStorage::kStorageSize> result;
  jbyte* src_bytes = env->GetByteArrayElements(array, nullptr);
  memcpy(&result[0], src_bytes, TaskTraitsExtensionStorage::kStorageSize);
  env->ReleaseByteArrayElements(array, src_bytes, JNI_ABORT);
  return result;
}
}  // namespace

// static
TaskTraits PostTaskAndroid::CreateTaskTraits(
    JNIEnv* env,
    jint priority,
    jboolean may_block,
    jboolean use_thread_pool,
    jbyte extension_id,
    const base::android::JavaParamRef<jbyteArray>& extension_data) {
  return TaskTraits(static_cast<TaskPriority>(priority), may_block,
                    use_thread_pool,
                    TaskTraitsExtensionStorage(
                        extension_id, GetExtensionData(env, extension_data)));
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
  // This could be run on any java thread, so we can't cache |env| in the
  // BindOnce because JNIEnv is thread specific.
  PostDelayedTask(
      FROM_HERE,
      PostTaskAndroid::CreateTaskTraits(env, priority, may_block,
                                        use_thread_pool, extension_id,
                                        extension_data),
      BindOnce(&PostTaskAndroid::RunJavaTask,
               base::android::ScopedJavaGlobalRef<jobject>(task),
               android::ConvertJavaStringToUTF8(runnable_class_name)),
      TimeDelta::FromMilliseconds(delay));
}

// static
void PostTaskAndroid::RunJavaTask(
    base::android::ScopedJavaGlobalRef<jobject> task,
    const std::string& runnable_class_name) {
  // JNIEnv is thread specific, but we don't know which thread we'll be run on
  // so we must look it up.
  std::string event_name = base::StrCat({"JniPostTask: ", runnable_class_name});
  TRACE_EVENT_BEGIN_WITH_FLAGS0(
      "toplevel", event_name.c_str(),
      TRACE_EVENT_FLAG_JAVA_STRING_LITERALS | TRACE_EVENT_FLAG_COPY);
  JNI_Runnable::Java_Runnable_run(base::android::AttachCurrentThread(), task);
  TRACE_EVENT_END_WITH_FLAGS0(
      "toplevel", event_name.c_str(),
      TRACE_EVENT_FLAG_JAVA_STRING_LITERALS | TRACE_EVENT_FLAG_COPY);
}

}  // namespace base
