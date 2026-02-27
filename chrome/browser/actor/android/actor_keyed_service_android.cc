// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/android/actor_keyed_service_android.h"

#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/actor/android/actor_task_android.h"
#include "chrome/browser/actor/android/jni_headers/ActorKeyedServiceFactory_jni.h"
#include "chrome/browser/actor/android/jni_headers/ActorKeyedService_jni.h"
#include "chrome/browser/profiles/profile.h"

using base::android::AttachCurrentThread;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaIntArray;

namespace actor {

namespace {
const char kActorKeyedServiceBridgeKey[] = "actor_keyed_service_bridge";
}  // namespace

ScopedJavaLocalRef<jobject> JNI_ActorKeyedServiceFactory_GetForProfile(
    JNIEnv* env,
    const JavaRef<jobject>& jprofile) {
  Profile* profile = Profile::FromJavaObject(jprofile);
  if (!profile) {
    return nullptr;
  }

  ActorKeyedService* service =
      ActorKeyedServiceFactory::GetActorKeyedService(profile);
  if (!service) {
    return nullptr;
  }

  ActorKeyedServiceAndroid* bridge = static_cast<ActorKeyedServiceAndroid*>(
      service->GetUserData(kActorKeyedServiceBridgeKey));
  if (!bridge) {
    service->SetUserData(kActorKeyedServiceBridgeKey,
                         std::make_unique<ActorKeyedServiceAndroid>(service));
    bridge = static_cast<ActorKeyedServiceAndroid*>(
        service->GetUserData(kActorKeyedServiceBridgeKey));
  }

  return bridge->GetJavaObject();
}

ActorKeyedServiceAndroid::ActorKeyedServiceAndroid(ActorKeyedService* service)
    : service_(service) {
  JNIEnv* env = AttachCurrentThread();
  java_obj_.Reset(
      env, Java_ActorKeyedService_create(env, reinterpret_cast<int64_t>(this)));

  task_state_subscription_ = service_->AddTaskStateChangedCallback(
      base::BindRepeating(&ActorKeyedServiceAndroid::OnTaskStateChanged,
                          base::Unretained(this)));
}

ActorKeyedServiceAndroid::~ActorKeyedServiceAndroid() {
  JNIEnv* env = AttachCurrentThread();
  Java_ActorKeyedService_clearNativePtr(env, java_obj_);
}

base::android::ScopedJavaLocalRef<jobject>
ActorKeyedServiceAndroid::GetJavaObject() {
  return ScopedJavaLocalRef<jobject>(java_obj_);
}

base::android::ScopedJavaLocalRef<jobjectArray>
ActorKeyedServiceAndroid::GetActiveTasks(JNIEnv* env) {
  std::vector<ScopedJavaLocalRef<jobject>> j_tasks;
  for (const auto& [id, task] : service_->GetActiveTasks()) {
    j_tasks.push_back(ActorTaskAndroid::GetForTask(const_cast<ActorTask*>(task))
                          ->GetJavaObject());
  }
  return base::android::ToJavaArrayOfObjects(env, j_tasks);
}

int32_t ActorKeyedServiceAndroid::GetActiveTasksCount(JNIEnv* env) {
  return static_cast<int32_t>(service_->GetActiveTasksCount());
}

base::android::ScopedJavaLocalRef<jobject> ActorKeyedServiceAndroid::GetTask(
    JNIEnv* env,
    int32_t task_id) {
  ActorTask* task = service_->GetTask(TaskId(task_id));
  if (!task) {
    return nullptr;
  }
  return ActorTaskAndroid::GetForTask(task)->GetJavaObject();
}

void ActorKeyedServiceAndroid::StopTask(JNIEnv* env,
                                        int32_t task_id,
                                        int32_t stop_reason) {
  service_->StopTask(TaskId(task_id),
                     static_cast<ActorTask::StoppedReason>(stop_reason));
}

void ActorKeyedServiceAndroid::OnTaskStateChanged(TaskId task_id,
                                                  ActorTask::State state) {
  JNIEnv* env = AttachCurrentThread();
  Java_ActorKeyedService_onTaskStateChanged(
      env, java_obj_, task_id.GetUnsafeValue(), static_cast<int>(state));
}

}  // namespace actor

DEFINE_JNI(ActorKeyedService)
DEFINE_JNI(ActorKeyedServiceFactory)
