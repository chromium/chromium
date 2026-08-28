// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/android/actor_task_android.h"

#include <vector>

#include "base/android/jni_string.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "third_party/jni_zero/default_conversions.h"

// Must come after headers that provide symbols used by @JniType.
#include "chrome/browser/actor/android/jni_headers/ActorTask_jni.h"

using base::android::ScopedJavaLocalRef;

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
                           env, reinterpret_cast<int64_t>(this),
                           task_->id().GetUnsafeValue(), task_->title(),
                           task_->GetProfile()->GetJavaObject()));
}

ActorTaskAndroid::~ActorTaskAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ActorTask_clearNativePtr(env, java_obj_);
}

ScopedJavaLocalRef<jobject> ActorTaskAndroid::GetJavaObject() {
  return ScopedJavaLocalRef<jobject>(java_obj_);
}

std::string ActorTaskAndroid::GetCurrentActionName() {
  return task_->step_progress();
}

int32_t ActorTaskAndroid::GetState() {
  return static_cast<int>(task_->GetState());
}

bool ActorTaskAndroid::IsCompleted() {
  return task_->IsCompleted();
}

bool ActorTaskAndroid::IsUnderActorControl() {
  return task_->IsUnderActorControl();
}

void ActorTaskAndroid::Pause() {
  task_->Pause(/*from_actor=*/false);
}

void ActorTaskAndroid::Resume() {
  task_->Resume();
}

std::vector<int32_t> ActorTaskAndroid::GetTabs() {
  auto tab_handles = task_->GetTabs();
  std::vector<int32_t> tab_ids;
  for (const auto& handle : tab_handles) {
    if (auto* tab_android = TabAndroid::FromTabHandle(handle)) {
      tab_ids.push_back(tab_android->GetAndroidId());
    }
  }
  return tab_ids;
}

std::vector<int32_t> ActorTaskAndroid::GetLastActedTabs() {
  auto tab_handles = task_->GetLastActedTabs();
  std::vector<int32_t> tab_ids;
  for (const auto& handle : tab_handles) {
    if (auto* tab_android = TabAndroid::FromTabHandle(handle)) {
      tab_ids.push_back(tab_android->GetAndroidId());
    }
  }
  return tab_ids;
}

}  // namespace actor

DEFINE_JNI(ActorTask)
