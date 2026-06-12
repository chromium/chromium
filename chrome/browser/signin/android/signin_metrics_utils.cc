// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "components/signin/public/base/signin_deep_link_metrics.h"
#include "components/signin/public/base/signin_metrics.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/signin/services/android/jni_headers/SigninMetricsUtils_jni.h"

static void JNI_SigninMetricsUtils_LogSigninUserActionForAccessPoint(
    JNIEnv* env,
    int32_t access_point) {
  signin_metrics::RecordSigninUserActionForAccessPoint(
      static_cast<signin_metrics::AccessPoint>(access_point));
}

static void JNI_SigninMetricsUtils_LogAccountConsistencyPromoAction(
    JNIEnv* env,
    int32_t consistency_promo_action,
    int32_t access_point) {
  CHECK_GE(consistency_promo_action, 0);
  CHECK_LE(consistency_promo_action,
           static_cast<int>(
               signin_metrics::AccountConsistencyPromoAction::kMaxValue));
  CHECK_GE(access_point, 0);
  CHECK_LE(access_point,
           static_cast<int>(signin_metrics::AccessPoint::kMaxValue));
  signin_metrics::RecordConsistencyPromoUserAction(
      static_cast<signin_metrics::AccountConsistencyPromoAction>(
          consistency_promo_action),
      static_cast<signin_metrics::AccessPoint>(access_point));
}

static void JNI_SigninMetricsUtils_LogSigninOffered(JNIEnv* env,
                                                    int32_t signin_promo_action,
                                                    int32_t access_point) {
  CHECK_GE(signin_promo_action, 0);
  CHECK_LE(signin_promo_action,
           static_cast<int>(signin_metrics::PromoAction::kMaxValue));
  CHECK_GE(access_point, 0);
  CHECK_LE(access_point,
           static_cast<int>(signin_metrics::AccessPoint::kMaxValue));
  signin_metrics::LogSignInOffered(
      static_cast<signin_metrics::AccessPoint>(access_point),
      static_cast<signin_metrics::PromoAction>(signin_promo_action));
}

static void JNI_SigninMetricsUtils_RecordCrossDeviceInitialState(
    JNIEnv* env,
    int32_t entry_point,
    int32_t state) {
  CHECK_GE(entry_point, 0);
  CHECK_LE(entry_point,
           static_cast<int>(signin::ExternalEntryPoint::kMaxValue));
  CHECK_GE(state, 0);
  CHECK_LE(state, static_cast<int>(
                      signin_metrics::CrossDeviceInitialState::kMaxValue));
  signin_metrics::RecordInitialState(
      static_cast<signin::ExternalEntryPoint>(entry_point),
      static_cast<signin_metrics::CrossDeviceInitialState>(state));
}

DEFINE_JNI(SigninMetricsUtils)
