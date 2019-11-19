// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_BACKGROUND_TASK_SCHEDULER_CHROME_BACKGROUND_TASK_FACTORY_H_
#define CHROME_BROWSER_ANDROID_BACKGROUND_TASK_SCHEDULER_CHROME_BACKGROUND_TASK_FACTORY_H_

// Intermediate that sets the ChromeBackgroundTaskFactory from C++. The C++ code
// does not call ChromeApplication (java initializations), so this setting was
// necessary for associating task ids with corresponding BackgroundTask classes
// for BackgroundTaskScheduler. This class can be used to set the default
// factory class to ChromeBackgroundTaskFactory by calling
// ChromeBackgroundTaskFactory::SetAsDefault().
class ChromeBackgroundTaskFactory {
 public:
  // Disable default constructor.
  ChromeBackgroundTaskFactory() = delete;

  // Disable copy (and move) semantics.
  ChromeBackgroundTaskFactory(const ChromeBackgroundTaskFactory&) = delete;
  ChromeBackgroundTaskFactory& operator=(const ChromeBackgroundTaskFactory&) =
      delete;

  ~ChromeBackgroundTaskFactory();

  // Sets the default factory implementation in //chrome for associating task
  // ids with corresponding BackgroundTask classes.
  static void SetAsDefault();
};

#endif  // CHROME_BROWSER_ANDROID_BACKGROUND_TASK_SCHEDULER_CHROME_BACKGROUND_TASK_FACTORY_H_
