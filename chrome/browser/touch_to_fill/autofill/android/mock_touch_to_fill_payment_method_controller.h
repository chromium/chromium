// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_MOCK_TOUCH_TO_FILL_PAYMENT_METHOD_CONTROLLER_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_MOCK_TOUCH_TO_FILL_PAYMENT_METHOD_CONTROLLER_H_

#include <jni.h>

#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/containers/span.h"
#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_payment_method_controller.h"
#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_payment_method_view.h"
#include "components/autofill/core/browser/data_model/payments/iban.h"
#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
#include "components/autofill/core/browser/integrators/touch_to_fill/touch_to_fill_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class MockTouchToFillPaymentMethodController
    : public TouchToFillPaymentMethodController {
 public:
  MockTouchToFillPaymentMethodController();

  MockTouchToFillPaymentMethodController(
      const MockTouchToFillPaymentMethodController&) = delete;
  MockTouchToFillPaymentMethodController& operator=(
      const MockTouchToFillPaymentMethodController&) = delete;

  ~MockTouchToFillPaymentMethodController() override;

  MOCK_METHOD(bool,
              ShowCreditCards,
              (std::unique_ptr<TouchToFillPaymentMethodView>,
               base::WeakPtr<TouchToFillDelegate>,
               base::span<const Suggestion>),
              (override));
  MOCK_METHOD(bool,
              ShowIbans,
              (std::unique_ptr<TouchToFillPaymentMethodView>,
               base::WeakPtr<TouchToFillDelegate>,
               base::span<const Iban>),
              (override));
  MOCK_METHOD(bool,
              ShowLoyaltyCards,
              (std::unique_ptr<TouchToFillPaymentMethodView>,
               base::WeakPtr<TouchToFillDelegate>,
               base::span<const LoyaltyCard>,
               base::span<const LoyaltyCard>,
               bool),
              (override));
  MOCK_METHOD(void, OnDismissed, (JNIEnv*, bool), (override));
  MOCK_METHOD(void, ScanCreditCard, (JNIEnv*), (override));
  MOCK_METHOD(void, ShowPaymentMethodSettings, (JNIEnv*), (override));
  MOCK_METHOD(void,
              CreditCardSuggestionSelected,
              (JNIEnv*, const base::android::JavaParamRef<jstring>&, bool),
              (override));
  MOCK_METHOD(void,
              LocalIbanSuggestionSelected,
              (JNIEnv*, const base::android::JavaParamRef<jstring>&),
              (override));
  MOCK_METHOD(void, ServerIbanSuggestionSelected, (JNIEnv*, long), (override));
  MOCK_METHOD(void,
              LoyaltyCardSuggestionSelected,
              (JNIEnv*, const std::string&),
              (override));
  MOCK_METHOD(int, GetJavaResourceId, (int), (override));
  MOCK_METHOD(base::android::ScopedJavaLocalRef<jobject>,
              GetJavaObject,
              (),
              (override));

  MOCK_METHOD(void, Hide, (), (override));
};

}  // namespace autofill

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_MOCK_TOUCH_TO_FILL_PAYMENT_METHOD_CONTROLLER_H_
