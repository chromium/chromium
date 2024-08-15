// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/supervised_user/aw_supervised_user_url_classifier.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "url/android/gurl_android.h"

#include "content/public/browser/browser_thread.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/AwSupervisedUserUrlClassifier_jni.h"

using base::android::AttachCurrentThread;

namespace android_webview {

AwSupervisedUserUrlClassifier::AwSupervisedUserUrlClassifier() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  platform_supports_url_checks_ =
      Java_AwSupervisedUserUrlClassifier_shouldCreateThrottle(env);
}

AwSupervisedUserUrlClassifier* AwSupervisedUserUrlClassifier::GetInstance() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  static base::NoDestructor<AwSupervisedUserUrlClassifier> instance;
  return instance.get();
}

bool AwSupervisedUserUrlClassifier::ShouldCreateThrottle() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // TODO(https://crbug.com/355528479): read this from prefs
  return platform_supports_url_checks_ && user_requires_url_checks_;
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

void AwSupervisedUserUrlClassifier::SetUserRequiresUrlChecks(
    bool user_requires_url_checks) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // TODO(https://crbug.com/355528479): persist this to prefs
  user_requires_url_checks_ = user_requires_url_checks;
}

static void JNI_AwSupervisedUserUrlClassifier_OnShouldBlockUrlResult(
    JNIEnv* env,
    jlong callback_id,
    jboolean shouldBlockUrl) {
  std::unique_ptr<UrlClassifierCallback> cb(
      reinterpret_cast<UrlClassifierCallback*>(callback_id));
  std::move(*cb).Run(shouldBlockUrl);
}

static void JNI_AwSupervisedUserUrlClassifier_SetUserRequiresUrlChecks(
    JNIEnv* env,
    jboolean user_requires_url_checks) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  AwSupervisedUserUrlClassifier::GetInstance()->SetUserRequiresUrlChecks(
      user_requires_url_checks);
}

}  // namespace android_webview
