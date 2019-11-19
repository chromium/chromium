// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/android/chrome_jni_headers/AfterStartupTaskUtils_jni.h"
#include "chrome/browser/after_startup_task_utils.h"

using base::android::JavaParamRef;

namespace android {

class AfterStartupTaskUtilsJNI {
 public:
  static void SetBrowserStartupIsComplete() {
    AfterStartupTaskUtils::SetBrowserStartupIsComplete();
  }
};

}  // android

static void JNI_AfterStartupTaskUtils_SetStartupComplete(JNIEnv* env) {
  android::AfterStartupTaskUtilsJNI::SetBrowserStartupIsComplete();
}
