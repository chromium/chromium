// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ANDROID_ACTOR_TASK_ANDROID_H_
#define CHROME_BROWSER_ACTOR_ANDROID_ACTOR_TASK_ANDROID_H_

#include <stdint.h>

#include <string>
#include <vector>

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

  std::string GetCurrentActionName();
  int32_t GetState();
  bool IsCompleted();
  bool IsUnderActorControl();
  void Pause();
  void Resume();
  std::vector<int32_t> GetTabs();
  std::vector<int32_t> GetLastActedTabs();
  int32_t GetLastActuatedTabId();

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;
  raw_ptr<ActorTask> task_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ANDROID_ACTOR_TASK_ANDROID_H_
