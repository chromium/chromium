// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/signin/signin_promo_util_android.h"

#include "base/android/jni_android.h"
#include "chrome/android/chrome_jni_headers/SigninPromoUtil_jni.h"
#include "ui/android/window_android.h"

namespace chrome {
namespace android {

// static
void SigninPromoUtilAndroid::StartSigninActivityForPromo(
    ui::WindowAndroid* window,
    signin_metrics::AccessPoint access_point) {
  if (window) {
    Java_SigninPromoUtil_openSigninActivityForPromo(
        base::android::AttachCurrentThread(), window->GetJavaObject(),
        jint(access_point));
  }
}

}  // namespace android
}  // namespace chrome
