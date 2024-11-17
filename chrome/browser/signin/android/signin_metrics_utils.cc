// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "components/signin/public/base/signin_metrics.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/signin/services/android/jni_headers/SigninMetricsUtils_jni.h"

static void JNI_SigninMetricsUtils_LogSigninUserActionForAccessPoint(
    JNIEnv* env,
    jint access_point) {
  signin_metrics::RecordSigninUserActionForAccessPoint(
      static_cast<signin_metrics::AccessPoint>(access_point));
}

static void JNI_SigninMetricsUtils_LogAccountConsistencyPromoAction(
    JNIEnv* env,
    jint promo_action,
    jint access_point) {
  CHECK_GE(promo_action, 0);
  CHECK_LE(promo_action,
           static_cast<int>(
               signin_metrics::AccountConsistencyPromoAction::kMaxValue));
  CHECK_GE(access_point, 0);
  CHECK_LE(access_point,
           static_cast<int>(signin_metrics::AccessPoint::ACCESS_POINT_MAX));
  signin_metrics::RecordConsistencyPromoUserAction(
      static_cast<signin_metrics::AccountConsistencyPromoAction>(promo_action),
      static_cast<signin_metrics::AccessPoint>(access_point));
}
