// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ANDROID_ACTOR_KEYED_SERVICE_ANDROID_H_
#define CHROME_BROWSER_ACTOR_ANDROID_ACTOR_KEYED_SERVICE_ANDROID_H_

#include <map>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"

namespace actor {

class ActorKeyedServiceAndroid : public base::SupportsUserData::Data {
 public:
  explicit ActorKeyedServiceAndroid(ActorKeyedService* service);
  ~ActorKeyedServiceAndroid() override;

  ActorKeyedServiceAndroid(const ActorKeyedServiceAndroid&) = delete;
  ActorKeyedServiceAndroid& operator=(const ActorKeyedServiceAndroid&) = delete;

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  base::android::ScopedJavaLocalRef<jobjectArray> GetActiveTasks(JNIEnv* env);
  int32_t GetActiveTasksCount(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jobject> GetTask(JNIEnv* env,
                                                     int32_t task_id);
  void StopTask(JNIEnv* env, int32_t task_id, int32_t stop_reason);

 private:
  void OnTaskStateChanged(TaskId task_id, ActorTask::State state);

  base::android::ScopedJavaGlobalRef<jobject> java_obj_;
  raw_ptr<ActorKeyedService> service_;
  base::CallbackListSubscription task_state_subscription_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ANDROID_ACTOR_KEYED_SERVICE_ANDROID_H_
