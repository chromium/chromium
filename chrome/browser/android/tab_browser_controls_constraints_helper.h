// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_TAB_BROWSER_CONTROLS_CONSTRAINTS_HELPER_H_
#define CHROME_BROWSER_ANDROID_TAB_BROWSER_CONTROLS_CONSTRAINTS_HELPER_H_

#include "base/android/scoped_java_ref.h"

// Dispatches changes to the browser controls constraints for a given tab.
class TabBrowserControlsConstraintsHelper {
 public:
  TabBrowserControlsConstraintsHelper(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  ~TabBrowserControlsConstraintsHelper();

  void UpdateState(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jweb_contents,
      jint constraints,
      jint current,
      jboolean animate,
      const base::android::JavaParamRef<jobject>& joffset_tags_info);
  void OnDestroyed(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj);

 private:
  base::android::ScopedJavaGlobalRef<jobject> jobj_;
};

#endif  // CHROME_BROWSER_ANDROID_TAB_BROWSER_CONTROLS_CONSTRAINTS_HELPER_H_
