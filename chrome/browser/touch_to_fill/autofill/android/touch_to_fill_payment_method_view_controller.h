// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_PAYMENT_METHOD_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_PAYMENT_METHOD_VIEW_CONTROLLER_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"

namespace autofill {
// An interface for interaction between the view and the corresponding UI
// controller on Android. Acts as the native counterpart for the Java
// TouchToFillPaymentMethodComponent.Delegate.
class TouchToFillPaymentMethodViewController {
 public:
  virtual ~TouchToFillPaymentMethodViewController() = default;

  // Called whenever the surface gets hidden (regardless of the cause).
  virtual void OnDismissed(JNIEnv* env, bool dismissed_by_user) = 0;
  // Calls credit card scanner
  virtual void ScanCreditCard(JNIEnv* env) = 0;
  // Causes the payment methods settings page to be shown
  virtual void ShowPaymentMethodSettings(JNIEnv* env) = 0;
  virtual void CreditCardSuggestionSelected(
      JNIEnv* env,
      base::android::JavaParamRef<jstring> unique_id,
      bool is_virtual) = 0;
  virtual void LocalIbanSuggestionSelected(
      JNIEnv* env,
      base::android::JavaParamRef<jstring> guid) = 0;
  virtual void ServerIbanSuggestionSelected(JNIEnv* env,
                                            long instrument_id) = 0;
  virtual base::android::ScopedJavaLocalRef<jobject> GetJavaObject() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_PAYMENT_METHOD_VIEW_CONTROLLER_H_
