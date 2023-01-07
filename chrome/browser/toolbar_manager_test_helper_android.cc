// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "toolbar_manager_test_helper_android.h"

#include "base/android/jni_android.h"
#include "chrome/test/test_support_jni_headers/ToolbarManagerTestHelper_jni.h"

namespace toolbar_manager {

void setSkipRecreateForTesting(bool skipRecreating) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ToolbarManagerTestHelper_setSkipRecreateForTesting(  // IN-TEST
      env, skipRecreating);
}

}  // namespace toolbar_manager
