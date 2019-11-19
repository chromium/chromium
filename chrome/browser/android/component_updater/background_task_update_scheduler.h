// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPONENT_UPDATER_BACKGROUND_TASK_UPDATE_SCHEDULER_H_
#define CHROME_BROWSER_ANDROID_COMPONENT_UPDATER_BACKGROUND_TASK_UPDATE_SCHEDULER_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/component_updater/update_scheduler.h"

namespace component_updater {

// Native-side implementation of the component update scheduler using the
// BackgroundTaskScheduler.
class BackgroundTaskUpdateScheduler : public UpdateScheduler {
 public:
  // Returns true if this scheduler can be used.
  static bool IsAvailable();

  BackgroundTaskUpdateScheduler();
  ~BackgroundTaskUpdateScheduler() override;

  // UpdateScheduler:
  void Schedule(const base::TimeDelta& initial_delay,
                const base::TimeDelta& delay,
                const UserTask& user_task,
                const OnStopTaskCallback& on_stop) override;
  void Stop() override;

  // JNI:
  void OnStartTask(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj);
  void OnStopTask(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

 private:
  void OnStartTaskDelayed();

  base::android::ScopedJavaGlobalRef<jobject> j_update_scheduler_;
  UserTask user_task_;
  OnStopTaskCallback on_stop_;

  base::WeakPtrFactory<BackgroundTaskUpdateScheduler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BackgroundTaskUpdateScheduler);
};

}  // namespace component_updater

#endif  // CHROME_BROWSER_ANDROID_COMPONENT_UPDATER_BACKGROUND_TASK_UPDATE_SCHEDULER_H_
