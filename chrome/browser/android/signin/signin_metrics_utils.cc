// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/signin/services/android/jni_headers/SigninMetricsUtils_jni.h"

static void JNI_SigninMetricsUtils_LogProfileAccountManagementMenu(
    JNIEnv* env,
    jint metric,
    jint gaia_service_type) {
  ProfileMetrics::LogProfileAndroidAccountManagementMenu(
      static_cast<ProfileMetrics::ProfileAndroidAccountManagementMenu>(metric),
      static_cast<signin::GAIAServiceType>(gaia_service_type));
}
