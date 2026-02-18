// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/android/actor_task_android.h"

#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/browser/actor/android/jni_headers/ActorTask_jni.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaIntArray;

namespace actor {

namespace {
const char kActorTaskBridgeKey[] = "actor_task_bridge";
}  // namespace

// static
ActorTaskAndroid* ActorTaskAndroid::GetForTask(ActorTask* task) {
  if (!task) {
    return nullptr;
  }
  ActorTaskAndroid* bridge =
      static_cast<ActorTaskAndroid*>(task->GetUserData(kActorTaskBridgeKey));
  if (!bridge) {
    task->SetUserData(kActorTaskBridgeKey,
                      std::make_unique<ActorTaskAndroid>(task));
    bridge =
        static_cast<ActorTaskAndroid*>(task->GetUserData(kActorTaskBridgeKey));
  }
  return bridge;
}

ActorTaskAndroid::ActorTaskAndroid(ActorTask* task) : task_(task) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(env, Java_ActorTask_Constructor(
                           env, reinterpret_cast<int64_t>(task_.get()),
                           task_->id().GetUnsafeValue(),
                           ConvertUTF8ToJavaString(env, task_->title())));
}

ActorTaskAndroid::~ActorTaskAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ActorTask_clearNativePtr(env, java_obj_);
}

ScopedJavaLocalRef<jobject> ActorTaskAndroid::GetJavaObject() {
  return ScopedJavaLocalRef<jobject>(java_obj_);
}

ScopedJavaLocalRef<jstring> ActorTaskAndroid::GetCurrentActionName(
    JNIEnv* env) {
  return ConvertUTF8ToJavaString(env, "");
}

int32_t ActorTaskAndroid::GetState(JNIEnv* env) {
  return static_cast<int>(task_->GetState());
}

bool ActorTaskAndroid::IsCompleted(JNIEnv* env) {
  return task_->IsCompleted();
}

bool ActorTaskAndroid::IsUnderActorControl(JNIEnv* env) {
  return task_->IsUnderActorControl();
}

void ActorTaskAndroid::Pause(JNIEnv* env) {
  task_->Pause(/*from_actor=*/false);
}

void ActorTaskAndroid::Resume(JNIEnv* env) {
  task_->Resume();
}

ScopedJavaLocalRef<jintArray> ActorTaskAndroid::GetTabs(JNIEnv* env) {
  auto tab_handles = task_->GetTabs();
  std::vector<int> tab_ids;
  for (const auto& handle : tab_handles) {
    tab_ids.push_back(handle.raw_value());
  }
  return ToJavaIntArray(env, tab_ids);
}

}  // namespace actor

DEFINE_JNI(ActorTask)
