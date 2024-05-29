// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/after_startup_task_utils.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/util/jni_headers/AfterStartupTaskUtils_jni.h"

using jni_zero::JavaParamRef;

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
