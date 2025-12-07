// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tabwindow/tab_window_manager_observer.h"

#include "base/android/jni_android.h"
#include "chrome/android/chrome_jni_headers/TabWindowManagerObserver_jni.h"

namespace tab_window {

TabWindowManagerObserver::TabWindowManagerObserver() {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(Java_TabWindowManagerObserver_create(
      env, reinterpret_cast<intptr_t>(this)));
}

TabWindowManagerObserver::~TabWindowManagerObserver() {
  Java_TabWindowManagerObserver_destroy(base::android::AttachCurrentThread(),
                                        java_obj_);
}

void TabWindowManagerObserver::OnTabStateInitialized(JNIEnv* env) {
  OnTabStateInitialized();
}

}  // namespace tab_window

DEFINE_JNI(TabWindowManagerObserver)
