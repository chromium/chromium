// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/android/actor_keyed_service_android.h"

#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/actor/android/actor_task_android.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "third_party/jni_zero/default_conversions.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/actor/android/jni_headers/ActorKeyedServiceFactory_jni.h"
#include "chrome/browser/actor/android/jni_headers/ActorKeyedService_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace actor {

namespace {
const char kActorKeyedServiceBridgeKey[] = "actor_keyed_service_bridge";
}  // namespace

ActorKeyedServiceAndroid* ActorKeyedServiceAndroid::Get(
    ActorKeyedService* service) {
  ActorKeyedServiceAndroid* bridge = static_cast<ActorKeyedServiceAndroid*>(
      service->GetUserData(kActorKeyedServiceBridgeKey));
  if (!bridge) {
    service->SetUserData(kActorKeyedServiceBridgeKey,
                         std::make_unique<ActorKeyedServiceAndroid>(service));
    bridge = static_cast<ActorKeyedServiceAndroid*>(
        service->GetUserData(kActorKeyedServiceBridgeKey));
  }
  return bridge;
}

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

  task_step_progress_subscription_ =
      service_->AddTaskStepProgressChangedCallback(base::BindRepeating(
          &ActorKeyedServiceAndroid::OnTaskStepProgressChanged,
          base::Unretained(this)));

  ensure_fgs_started_subscription_ =
      service_->AddForegroundServiceStartedCallback(base::BindRepeating(
          &ActorKeyedServiceAndroid::EnsureForegroundServiceStarted,
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

std::vector<jni_zero::ScopedJavaLocalRef<jobject>>
ActorKeyedServiceAndroid::GetActiveTasks() {
  std::vector<ScopedJavaLocalRef<jobject>> j_tasks;
  for (const auto& [id, task] : service_->GetActiveTasks()) {
    j_tasks.push_back(ActorTaskAndroid::GetForTask(const_cast<ActorTask*>(task))
                          ->GetJavaObject());
  }
  return j_tasks;
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

void ActorKeyedServiceAndroid::SetPreparedBackgroundTab(
    TabAndroid* tab,
    const std::string& glic_trigger_message_id) {
  service_->NotifyBackgroundTabReady(tab, glic_trigger_message_id);
}

void ActorKeyedServiceAndroid::NotifyBackgroundSetupFailed(
    const std::string& glic_trigger_message_id) {
  service_->NotifyBackgroundSetupFailed(glic_trigger_message_id);
}

void ActorKeyedServiceAndroid::OnTaskStateChanged(ActorTask& task) {
  JNIEnv* env = AttachCurrentThread();
  Java_ActorKeyedService_onTaskStateChanged(env, java_obj_,
                                            task.id().GetUnsafeValue(),
                                            static_cast<int>(task.GetState()));
}

void ActorKeyedServiceAndroid::OnTaskStepProgressChanged(
    ActorTask& task,
    const std::string& step_progress) {
  JNIEnv* env = AttachCurrentThread();
  Java_ActorKeyedService_onTaskStepProgressChanged(
      env, java_obj_, task.id().GetUnsafeValue(),
      base::android::ConvertUTF8ToJavaString(env, step_progress));
}

void ActorKeyedServiceAndroid::EnsureForegroundServiceStarted(
    const std::string& glic_trigger_message_id) {
  JNIEnv* env = AttachCurrentThread();
  Java_ActorKeyedService_ensureForegroundServiceStarted(
      env, java_obj_, glic_trigger_message_id);
}

void CreateBackgroundTabForTask(
    Profile* profile,
    TaskId task_id,
    ActorKeyedService::CreateActorTabCallback callback) {
  ActorKeyedService* service =
      ActorKeyedServiceFactory::GetActorKeyedService(profile);
  if (!service) {
    std::move(callback).Run(nullptr);
    return;
  }
  ActorKeyedServiceAndroid* bridge = ActorKeyedServiceAndroid::Get(service);
  JNIEnv* env = AttachCurrentThread();
  auto j_callback = base::android::ToJniCallback(
      env, base::BindOnce(
               [](ActorKeyedService::CreateActorTabCallback callback,
                  const base::android::JavaRef<jobject>& j_tab) {
                 tabs::TabInterface* tab = nullptr;
                 if (j_tab) {
                   if (TabAndroid* tab_android = TabAndroid::GetNativeTab(
                           AttachCurrentThread(), j_tab)) {
                     tab = tab_android;
                   }
                 }
                 std::move(callback).Run(tab);
               },
               std::move(callback)));
  Java_ActorKeyedService_createBackgroundTabForTask(
      env, bridge->GetJavaObject(), profile, task_id.value(), j_callback);
}

}  // namespace actor

DEFINE_JNI(ActorKeyedService)
DEFINE_JNI(ActorKeyedServiceFactory)
