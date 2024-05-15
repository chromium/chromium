// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ACCOUNT_STORAGE_NOTICE_COORDINATOR_OBSERVER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ACCOUNT_STORAGE_NOTICE_COORDINATOR_OBSERVER_H_

#include <jni.h>

// JNI interface to observe Coordinator.java events on C++.
class CoordinatorObserver {
 public:
  virtual ~CoordinatorObserver() = default;

  virtual void OnClosed(JNIEnv* env) = 0;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ACCOUNT_STORAGE_NOTICE_COORDINATOR_OBSERVER_H_
