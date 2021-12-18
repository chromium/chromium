// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "chrome/browser/share/android/jni_headers/LinkToTextBridge_jni.h"
#include "components/shared_highlighting/core/common/disabled_sites.h"
#include "components/shared_highlighting/core/common/shared_highlighting_metrics.h"
#include "url/android/gurl_android.h"

using shared_highlighting::LinkGenerationReadyStatus;
using shared_highlighting::LinkGenerationStatus;

// TODO(gayane): Update the name whenever
// |ShouldOfferLinkToText| updated to more descriptive
// name.
static jboolean JNI_LinkToTextBridge_ShouldOfferLinkToText(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_url) {
  std::unique_ptr<GURL> url = url::GURLAndroid::ToNativeGURL(env, j_url);
  return shared_highlighting::ShouldOfferLinkToText(*url);
}

static void JNI_LinkToTextBridge_LogFailureMetrics(JNIEnv* env, jint error) {
  shared_highlighting::LogRequestedFailureMetrics(
      static_cast<shared_highlighting::LinkGenerationError>(error));
}

static void JNI_LinkToTextBridge_LogSuccessMetrics(JNIEnv* env) {
  shared_highlighting::LogRequestedSuccessMetrics();
}

static void JNI_LinkToTextBridge_LogLinkRequestedBeforeStatus(
    JNIEnv* env,
    jint status,
    jint ready_status) {
  shared_highlighting::LogLinkRequestedBeforeStatus(
      static_cast<LinkGenerationStatus>(status),
      static_cast<LinkGenerationReadyStatus>(ready_status));
}
