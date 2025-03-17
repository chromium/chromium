// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_ANDROID_TASK_MANAGER_OBSERVER_ANDROID_H_
#define CHROME_BROWSER_TASK_MANAGER_ANDROID_TASK_MANAGER_OBSERVER_ANDROID_H_

#include <stdint.h>

#include <vector>

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/task_manager/task_manager_observer.h"

namespace task_manager {

using TaskId = int64_t;
using TaskIdList = std::vector<TaskId>;

class TaskManagerObserverAndroid : public TaskManagerObserver {
 public:
  TaskManagerObserverAndroid(JNIEnv* env,
                             const jni_zero::JavaRef<jobject>& obj,
                             int refresh_time_millis,
                             task_manager::RefreshType resource_flags);
  ~TaskManagerObserverAndroid() override;

  void OnTaskAdded(TaskId id) override;
  void OnTaskToBeRemoved(TaskId id) override;
  void OnTasksRefreshed(const TaskIdList& task_ids) override;
  void OnTasksRefreshedWithBackgroundCalculations(
      const TaskIdList& task_ids) override;
  void OnTaskUnresponsive(TaskId id) override;

  jni_zero::ScopedJavaGlobalRef<jobject> java_object_ = nullptr;
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_ANDROID_TASK_MANAGER_OBSERVER_ANDROID_H_
