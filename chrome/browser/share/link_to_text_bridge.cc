// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "chrome/browser/share/android/jni_headers/LinkToTextBridge_jni.h"
#include "components/shared_highlighting/core/common/disabled_sites.h"
#include "components/shared_highlighting/core/common/shared_highlighting_metrics.h"
#include "url/android/gurl_android.h"

static void JNI_LinkToTextBridge_LogGenerateErrorTabHidden(JNIEnv* env) {
  shared_highlighting::LogGenerateErrorTabHidden();
}

static void JNI_LinkToTextBridge_LogGenerateErrorOmniboxNavigation(
    JNIEnv* env) {
  shared_highlighting::LogGenerateErrorOmniboxNavigation();
}

static void JNI_LinkToTextBridge_LogGenerateErrorTabCrash(JNIEnv* env) {
  shared_highlighting::LogGenerateErrorTabCrash();
}

static void JNI_LinkToTextBridge_LogGenerateErrorIFrame(JNIEnv* env) {
  shared_highlighting::LogGenerateErrorIFrame();
}

static void JNI_LinkToTextBridge_LogGenerateErrorBlockList(JNIEnv* env) {
  shared_highlighting::LogGenerateErrorBlockList();
}

static void JNI_LinkToTextBridge_LogGenerateErrorTimeout(JNIEnv* env) {
  shared_highlighting::LogGenerateErrorTimeout();
}

// TODO(gayane): Update the name whenever
// |shared_highlighting::ShouldOfferLinkToText| updated to more descriptive
// name.
static jboolean JNI_LinkToTextBridge_ShouldOfferLinkToText(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_url) {
  std::unique_ptr<GURL> url = url::GURLAndroid::ToNativeGURL(env, j_url);
  return shared_highlighting::ShouldOfferLinkToText(*url);
}
