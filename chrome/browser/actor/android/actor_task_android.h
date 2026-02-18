// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ANDROID_ACTOR_TASK_ANDROID_H_
#define CHROME_BROWSER_ACTOR_ANDROID_ACTOR_TASK_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "chrome/browser/actor/actor_task.h"

namespace actor {

class ActorTaskAndroid : public base::SupportsUserData::Data {
 public:
  static ActorTaskAndroid* GetForTask(ActorTask* task);

  explicit ActorTaskAndroid(ActorTask* task);
  ~ActorTaskAndroid() override;

  ActorTaskAndroid(const ActorTaskAndroid&) = delete;
  ActorTaskAndroid& operator=(const ActorTaskAndroid&) = delete;

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  base::android::ScopedJavaLocalRef<jstring> GetCurrentActionName(JNIEnv* env);
  int32_t GetState(JNIEnv* env);
  bool IsCompleted(JNIEnv* env);
  bool IsUnderActorControl(JNIEnv* env);
  void Pause(JNIEnv* env);
  void Resume(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jintArray> GetTabs(JNIEnv* env);

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;
  raw_ptr<ActorTask> task_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ANDROID_ACTOR_TASK_ANDROID_H_
