// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/preferences/autofill/autofill_payment_methods_delegate.h"

#include "chrome/android/chrome_jni_headers/AutofillPaymentMethodsDelegate_jni.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"

#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_flow.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"

using base::android::JavaParamRef;

namespace autofill {

AutofillPaymentMethodsDelegate::AutofillPaymentMethodsDelegate(Profile* profile)
    : profile_(profile) {}

AutofillPaymentMethodsDelegate::~AutofillPaymentMethodsDelegate() = default;

// Initializes an instance of AutofillPaymentMethodsDelegate from the
// Java side.
static jlong JNI_AutofillPaymentMethodsDelegate_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile) {
  AutofillPaymentMethodsDelegate* instance = new AutofillPaymentMethodsDelegate(
      ProfileAndroid::FromProfileAndroid(j_profile));
  return reinterpret_cast<intptr_t>(instance);
}

void AutofillPaymentMethodsDelegate::Cleanup(JNIEnv* env) {
  delete this;
}

void AutofillPaymentMethodsDelegate::OfferVirtualCardEnrollment(
    JNIEnv* env,
    int64_t instrumentId) {
  // TODO (crbug/1281695) Implement call to enroll Virtual Cards
}

void AutofillPaymentMethodsDelegate::UnenrollVirtualCard(JNIEnv* env,
                                                         int64_t instrumentId) {
  // TODO (crbug/1281695) Implement call to unenroll Virtual Cards
}
}  // namespace autofill
