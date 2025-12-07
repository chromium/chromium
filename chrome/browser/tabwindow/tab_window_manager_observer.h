// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TABWINDOW_TAB_WINDOW_MANAGER_OBSERVER_H_
#define CHROME_BROWSER_TABWINDOW_TAB_WINDOW_MANAGER_OBSERVER_H_

#include "base/android/jni_android.h"
#include "base/observer_list_types.h"

namespace tab_window {

// Native counterpart of TabWindowManagerObserver.java.
// This class observes TabWindowManager on the Java side and notifies native
// observers.
class TabWindowManagerObserver {
 public:
  TabWindowManagerObserver();
  ~TabWindowManagerObserver();
  TabWindowManagerObserver(const TabWindowManagerObserver&) = delete;
  TabWindowManagerObserver& operator=(const TabWindowManagerObserver&) = delete;

  // Methods called from Java via JNI.
  void OnTabStateInitialized(JNIEnv* env);

  // TabWindowManager.Observer methods.
  virtual void OnTabStateInitialized() = 0;

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;
};

}  // namespace tab_window

#endif  // CHROME_BROWSER_TABWINDOW_TAB_WINDOW_MANAGER_OBSERVER_H_
