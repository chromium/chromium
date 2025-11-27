// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_PAYMENT_METHOD_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_PAYMENT_METHOD_VIEW_CONTROLLER_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"

namespace autofill {

class LoyaltyCard;

// An interface for interaction between the view and the corresponding UI
// controller on Android. Acts as the native counterpart for the Java
// TouchToFillPaymentMethodComponent.Delegate.
class TouchToFillPaymentMethodViewController {
 public:
  virtual ~TouchToFillPaymentMethodViewController() = default;

  // Called whenever the surface gets hidden (regardless of the cause).
  virtual void OnDismissed(JNIEnv* env,
                           bool dismissed_by_user,
                           bool should_reshow) = 0;
  // Calls credit card scanner
  virtual void ScanCreditCard(JNIEnv* env) = 0;
  // Causes the payment methods settings page to be shown
  virtual void ShowPaymentMethodSettings(JNIEnv* env) = 0;
  virtual void CreditCardSuggestionSelected(JNIEnv* env,
                                            const std::string& unique_id,
                                            bool is_virtual) = 0;
  virtual void BnplSuggestionSelected(
      JNIEnv* env,
      std::optional<int64_t> extracted_amount) = 0;
  virtual void LocalIbanSuggestionSelected(JNIEnv* env,
                                           const std::string& guid) = 0;
  virtual void ServerIbanSuggestionSelected(JNIEnv* env,
                                            long instrument_id) = 0;
  // Called when the user taps on a loyalty card in the payments TTF bottom
  // sheet.
  virtual void LoyaltyCardSuggestionSelected(
      JNIEnv* env,
      const LoyaltyCard& loyalty_card) = 0;
  // Called when the user taps on a BNPL issuer in the BNPL issuer selection
  // bottom sheet.
  virtual void OnBnplIssuerSuggestionSelected(JNIEnv* env,
                                              const std::string& issuer_id) = 0;
  // Called when the user accepted the terms of service for linking a BNPL
  // issuer.
  virtual void OnBnplTosAccepted(JNIEnv* env) = 0;
  virtual int GetJavaResourceId(int native_resource_id) const = 0;
  virtual base::android::ScopedJavaLocalRef<jobject> GetJavaObject() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_PAYMENT_METHOD_VIEW_CONTROLLER_H_
