// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SIGNIN_SIGNIN_PROMO_UTIL_ANDROID_H_
#define CHROME_BROWSER_ANDROID_SIGNIN_SIGNIN_PROMO_UTIL_ANDROID_H_

#include "base/android/jni_android.h"
#include "components/signin/public/base/signin_metrics.h"

namespace ui {
class WindowAndroid;
}

namespace chrome {
namespace android {

class SigninPromoUtilAndroid {
 public:
  // Opens a signin flow with the specified |access_point| for metrics.
  static void StartSigninActivityForPromo(
      ui::WindowAndroid* window,
      signin_metrics::AccessPoint access_point);
};

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_SIGNIN_SIGNIN_PROMO_UTIL_ANDROID_H_
