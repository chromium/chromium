// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/android/advanced_protection_status_manager_test_util.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "chrome/browser/safe_browsing/android/test_support_jni/AdvancedProtectionStatusManagerTestUtil_jni.h"

namespace safe_browsing {

void SetAdvancedProtectionStateForTesting(
    bool is_advanced_protection_requested_by_os) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AdvancedProtectionStatusManagerTestUtil_setOsAdvancedProtectionStateForTesting(
      env, is_advanced_protection_requested_by_os);
}

}  // namespace safe_browsing
