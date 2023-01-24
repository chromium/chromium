// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_origin_verification_scheduler_bridge.h"

#include <string>
#include "android_webview/browser_jni_headers/AwOriginVerificationSchedulerBridge_jni.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/no_destructor.h"

namespace android_webview {

// static
AwOriginVerificationSchedulerBridge*
AwOriginVerificationSchedulerBridge::GetInstance() {
  static base::NoDestructor<AwOriginVerificationSchedulerBridge> instance;
  return instance.get();
}

void AwOriginVerificationSchedulerBridge::Verify(
    std::string url,
    OriginVerifierCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  JNIEnv* env = base::android::AttachCurrentThread();
  intptr_t callback_id = reinterpret_cast<intptr_t>(
      new OriginVerifierCallback(std::move(callback)));

  auto j_url = base::android::ConvertUTF8ToJavaString(env, url);
  Java_AwOriginVerificationSchedulerBridge_verify(env, j_url, callback_id);
}

static void JNI_AwOriginVerificationSchedulerBridge_OnVerificationResult(
    JNIEnv* env,
    jlong callback_id,
    jboolean verified) {
  std::unique_ptr<OriginVerifierCallback> cb(
      reinterpret_cast<OriginVerifierCallback*>(callback_id));
  std::move(*cb).Run(verified);
}

}  // namespace android_webview
