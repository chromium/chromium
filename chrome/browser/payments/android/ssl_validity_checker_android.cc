// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/android/chrome_jni_headers/SslValidityChecker_jni.h"
#include "chrome/browser/payments/ssl_validity_checker.h"
#include "content/public/browser/web_contents.h"

namespace payments {

// static
base::android::ScopedJavaLocalRef<jstring>
JNI_SslValidityChecker_GetInvalidSslCertificateErrorMessage(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jweb_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  return base::android::ConvertUTF8ToJavaString(
      env,
      SslValidityChecker::GetInvalidSslCertificateErrorMessage(web_contents));
}

// static
jboolean JNI_SslValidityChecker_IsValidPageInPaymentHandlerWindow(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jweb_contents) {
  return SslValidityChecker::IsValidPageInPaymentHandlerWindow(
      content::WebContents::FromJavaWebContents(jweb_contents));
}

}  // namespace payments
