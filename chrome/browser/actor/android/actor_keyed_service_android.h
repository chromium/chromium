// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ANDROID_ACTOR_KEYED_SERVICE_ANDROID_H_
#define CHROME_BROWSER_ACTOR_ANDROID_ACTOR_KEYED_SERVICE_ANDROID_H_

#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/supports_user_data.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"

class TabAndroid;

namespace actor {

class ActorKeyedServiceAndroid : public base::SupportsUserData::Data {
 public:
  static ActorKeyedServiceAndroid* Get(ActorKeyedService* service);

  explicit ActorKeyedServiceAndroid(ActorKeyedService* service);
  ~ActorKeyedServiceAndroid() override;

  ActorKeyedServiceAndroid(const ActorKeyedServiceAndroid&) = delete;
  ActorKeyedServiceAndroid& operator=(const ActorKeyedServiceAndroid&) = delete;

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  std::vector<jni_zero::ScopedJavaLocalRef<jobject>> GetActiveTasks();
  int32_t GetActiveTasksCount(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jobject> GetTask(JNIEnv* env,
                                                     int32_t task_id);
  void StopTask(JNIEnv* env, int32_t task_id, int32_t stop_reason);

  // Called by JNI.
  void SetPreparedBackgroundTab(TabAndroid* tab,
                                const std::string& glic_trigger_message_id);

  void NotifyBackgroundSetupFailed(const std::string& glic_trigger_message_id);

 private:
  void OnTaskStateChanged(ActorTask& task);
  void OnTaskStepProgressChanged(ActorTask& task,
                                 const std::string& step_progress);
  void EnsureForegroundServiceStarted(
      const std::string& glic_trigger_message_id);

  base::android::ScopedJavaGlobalRef<jobject> java_obj_;
  raw_ptr<ActorKeyedService> service_;
  base::CallbackListSubscription task_state_subscription_;
  base::CallbackListSubscription task_step_progress_subscription_;
  base::CallbackListSubscription ensure_fgs_started_subscription_;
};

void CreateBackgroundTabForTask(
    Profile* profile,
    TaskId task_id,
    ActorKeyedService::CreateActorTabCallback callback);

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ANDROID_ACTOR_KEYED_SERVICE_ANDROID_H_
