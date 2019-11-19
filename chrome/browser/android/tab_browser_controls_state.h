// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_TAB_BROWSER_CONTROLS_STATE_H_
#define CHROME_BROWSER_ANDROID_TAB_BROWSER_CONTROLS_STATE_H_

#include "base/android/scoped_java_ref.h"

// Native TabBrowsreControlsState. Managed by Java class.
class TabBrowserControlsState {
 public:
  TabBrowserControlsState(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj);
  ~TabBrowserControlsState();

  void UpdateState(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj,
                   const base::android::JavaParamRef<jobject>& jweb_contents,
                   jint constraints,
                   jint current,
                   jboolean animate);
  void OnDestroyed(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj);

 private:
  base::android::ScopedJavaGlobalRef<jobject> jobj_;
};

#endif  // CHROME_BROWSER_ANDROID_TAB_BROWSER_CONTROLS_STATE_H_
