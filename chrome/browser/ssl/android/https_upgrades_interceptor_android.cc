// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/https_upgrades_interceptor.h"

#include <stdint.h>

#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ssl/android/jni_headers/HttpsUpgradesInterceptor_jni.h"

static void JNI_HttpsUpgradesInterceptor_SetHttpsPortForTesting(  // IN-TEST
    JNIEnv* env,
    int32_t port) {
  HttpsUpgradesInterceptor::SetHttpsPortForTesting(port);  // IN-TEST
}

static void JNI_HttpsUpgradesInterceptor_SetHttpPortForTesting(  // IN-TEST
    JNIEnv* env,
    int32_t port) {
  HttpsUpgradesInterceptor::SetHttpPortForTesting(port);  // IN-TEST
}

DEFINE_JNI(HttpsUpgradesInterceptor)
