// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_MOCK_TOUCH_TO_FILL_PAYMENT_METHOD_CONTROLLER_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_MOCK_TOUCH_TO_FILL_PAYMENT_METHOD_CONTROLLER_H_

#include <jni.h>

#include <optional>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_payment_method_controller.h"
#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_payment_method_view.h"
#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/data_model/payments/iban.h"
#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
#include "components/autofill/core/browser/integrators/touch_to_fill/touch_to_fill_delegate.h"
#include "components/autofill/core/browser/payments/bnpl_util.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/ui/payments/bnpl_tos_controller.h"
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
              ShowPaymentMethods,
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
  MOCK_METHOD(
      bool,
      OnPurchaseAmountExtracted,
      (base::span<const payments::BnplIssuerContext> bnpl_issuer_contexts,
       std::optional<int64_t> extracted_amount,
       bool is_amount_supported_by_any_issuer,
       const std::optional<std::string>& app_locale,
       base::OnceCallback<void(BnplIssuer)> selected_issuer_callback,
       base::OnceClosure cancel_callback),
      (override));
  MOCK_METHOD(bool,
              ShowProgressScreen,
              (std::unique_ptr<TouchToFillPaymentMethodView> view,
               base::OnceClosure cancel_callback),
              (override));
  MOCK_METHOD(
      bool,
      ShowBnplIssuers,
      (base::span<const payments::BnplIssuerContext> bnpl_issuer_contexts,
       const std::string& app_locale,
       base::OnceCallback<void(BnplIssuer)> selected_issuer_callback,
       base::OnceClosure cancel_callback),
      (override));
  MOCK_METHOD(bool,
              ShowErrorScreen,
              (std::unique_ptr<TouchToFillPaymentMethodView> view,
               const std::u16string& title,
               const std::u16string& description),
              (override));
  MOCK_METHOD(bool,
              ShowBnplIssuerTos,
              (BnplTosModel bnpl_tos_model,
               base::OnceClosure accept_callback,
               base::OnceClosure cancel_callback),
              (override));
  MOCK_METHOD(void,
              OnDismissed,
              (JNIEnv*, bool dismissed_by_user, bool should_reshow),
              (override));
  MOCK_METHOD(void, ScanCreditCard, (JNIEnv*), (override));
  MOCK_METHOD(void, ShowPaymentMethodSettings, (JNIEnv*), (override));
  MOCK_METHOD(void,
              CreditCardSuggestionSelected,
              (JNIEnv*, const std::string&, bool),
              (override));
  MOCK_METHOD(void,
              BnplSuggestionSelected,
              (JNIEnv*, std::optional<int64_t>),
              (override));
  MOCK_METHOD(void,
              LocalIbanSuggestionSelected,
              (JNIEnv*, const std::string&),
              (override));
  MOCK_METHOD(void, ServerIbanSuggestionSelected, (JNIEnv*, long), (override));
  MOCK_METHOD(void,
              LoyaltyCardSuggestionSelected,
              (JNIEnv*, const LoyaltyCard&),
              (override));
  MOCK_METHOD(void,
              OnBnplIssuerSuggestionSelected,
              (JNIEnv*, const std::string&),
              (override));
  MOCK_METHOD(void, OnBnplTosAccepted, (JNIEnv*), (override));
  MOCK_METHOD(int, GetJavaResourceId, (int), (const, override));
  MOCK_METHOD(base::android::ScopedJavaLocalRef<jobject>,
              GetJavaObject,
              (),
              (override));

  MOCK_METHOD(void, Hide, (), (override));
  MOCK_METHOD(void, SetVisible, (bool visible), (override));
};

}  // namespace autofill

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_MOCK_TOUCH_TO_FILL_PAYMENT_METHOD_CONTROLLER_H_
