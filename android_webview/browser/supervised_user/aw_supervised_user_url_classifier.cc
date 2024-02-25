// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/supervised_user/aw_supervised_user_url_classifier.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "url/android/gurl_android.h"

#include "android_webview/browser_jni_headers/AwSupervisedUserUrlClassifier_jni.h"
#include "content/public/browser/browser_thread.h"

using base::android::AttachCurrentThread;

namespace android_webview {

AwSupervisedUserUrlClassifier::AwSupervisedUserUrlClassifier() {
  JNIEnv* env = AttachCurrentThread();
  shouldCreateThrottle_ =
      Java_AwSupervisedUserUrlClassifier_shouldCreateThrottle(env);
}

AwSupervisedUserUrlClassifier* AwSupervisedUserUrlClassifier::GetInstance() {
  static base::NoDestructor<AwSupervisedUserUrlClassifier> instance;
  return instance.get();
}

bool AwSupervisedUserUrlClassifier::ShouldCreateThrottle() {
  return shouldCreateThrottle_;
}

void AwSupervisedUserUrlClassifier::ShouldBlockUrl(
    const GURL& request_url,
    UrlClassifierCallback callback) {
  JNIEnv* env = AttachCurrentThread();
  auto request_url_java = url::GURLAndroid::FromNativeGURL(env, request_url);
  intptr_t callback_id = reinterpret_cast<intptr_t>(
      new UrlClassifierCallback(std::move(callback)));

  return Java_AwSupervisedUserUrlClassifier_shouldBlockUrl(
      env, request_url_java, callback_id);
}

static void JNI_AwSupervisedUserUrlClassifier_OnShouldBlockUrlResult(
    JNIEnv* env,
    jlong callback_id,
    jboolean shouldBlockUrl) {
  std::unique_ptr<UrlClassifierCallback> cb(
      reinterpret_cast<UrlClassifierCallback*>(callback_id));
  std::move(*cb).Run(shouldBlockUrl);
}

}  // namespace android_webview
