// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_ANDROID_CONTEXTUAL_TASKS_CONTROL_COORDINATOR_ANDROID_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_ANDROID_CONTEXTUAL_TASKS_CONTROL_COORDINATOR_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "base/scoped_observation.h"
#include "base/uuid.h"
#include "chrome/browser/profiles/profile.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"

namespace contextual_tasks {

class ContextualTasksControlCoordinatorAndroid
    : public ContextualTasksService::Observer {
 public:
  ContextualTasksControlCoordinatorAndroid(
      JNIEnv* env,
      const jni_zero::JavaRef<jobject>& obj,
      Profile* profile);
  ~ContextualTasksControlCoordinatorAndroid() override;

  ContextualTasksControlCoordinatorAndroid(
      const ContextualTasksControlCoordinatorAndroid&) = delete;
  ContextualTasksControlCoordinatorAndroid& operator=(
      const ContextualTasksControlCoordinatorAndroid&) = delete;

  void Destroy(JNIEnv* env);

  // ContextualTasksService::Observer implementation.
  void OnTaskAdded(const ContextualTask& task,
                   ContextualTasksService::TriggerSource source) override;
  void OnTaskUpdated(const ContextualTask& task,
                     ContextualTasksService::TriggerSource source) override;
  void OnTaskRemoved(const base::Uuid& task_id,
                     ContextualTasksService::TriggerSource source) override;
  void OnWillBeDestroyed() override;

 private:
  jni_zero::ScopedJavaGlobalRef<jobject> java_obj_;
  base::ScopedObservation<ContextualTasksService,
                          ContextualTasksService::Observer>
      scoped_observation_{this};
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_ANDROID_CONTEXTUAL_TASKS_CONTROL_COORDINATOR_ANDROID_H_
