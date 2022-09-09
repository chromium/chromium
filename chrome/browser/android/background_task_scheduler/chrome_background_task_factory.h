// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_BACKGROUND_TASK_SCHEDULER_CHROME_BACKGROUND_TASK_FACTORY_H_
#define CHROME_BROWSER_ANDROID_BACKGROUND_TASK_SCHEDULER_CHROME_BACKGROUND_TASK_FACTORY_H_

#include "components/background_task_scheduler/background_task.h"

// Given a task id, creates the corresponding BackgroundTask.
// Also provides methods to initialize the Java factory from native.
class ChromeBackgroundTaskFactory {
 public:
  // Disable default constructor.
  ChromeBackgroundTaskFactory() = delete;

  // Disable copy (and move) semantics.
  ChromeBackgroundTaskFactory(const ChromeBackgroundTaskFactory&) = delete;
  ChromeBackgroundTaskFactory& operator=(const ChromeBackgroundTaskFactory&) =
      delete;

  ~ChromeBackgroundTaskFactory();

  // Sets the ChromeBackgroundTaskFactory in Java. The C++ code
  // does not call ChromeApplication (java initializations), so this setting was
  // necessary for associating task ids with corresponding BackgroundTask
  // classes for BackgroundTaskScheduler. This method can be used to set the
  // default factory class to ChromeBackgroundTaskFactory.
  static void SetAsDefault();

  // Creates and returns a BackgroundTask for the given |task_id|.
  static std::unique_ptr<background_task::BackgroundTask>
  GetNativeBackgroundTaskFromTaskId(int task_id);
};

#endif  // CHROME_BROWSER_ANDROID_BACKGROUND_TASK_SCHEDULER_CHROME_BACKGROUND_TASK_FACTORY_H_
