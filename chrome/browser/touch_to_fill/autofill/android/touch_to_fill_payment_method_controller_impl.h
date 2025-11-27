// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_PAYMENT_METHOD_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_PAYMENT_METHOD_CONTROLLER_IMPL_H_

#include <memory>
#include <optional>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_payment_method_controller.h"
#include "components/autofill/android/touch_to_fill_keyboard_suppressor.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"

namespace autofill {

namespace payments {
struct BnplIssuerContext;
}  // namespace payments

class BnplIssuer;
struct BnplTosModel;
class ContentAutofillClient;
class Iban;
class LoyaltyCard;
class TouchToFillDelegate;
class TouchToFillPaymentMethodView;

// Controller of the bottom sheet surface for filling credit card IBAN or
// loyalty cards on Android. It is responsible for showing the view and handling
// user interactions. While the surface is shown, stores its Java counterpart in
// `java_object_`.
class TouchToFillPaymentMethodControllerImpl
    : public TouchToFillPaymentMethodController,
      public ContentAutofillDriverFactory::Observer,
      public content::WebContentsObserver {
 public:
  explicit TouchToFillPaymentMethodControllerImpl(
      ContentAutofillClient* autofill_client);
  TouchToFillPaymentMethodControllerImpl(
      const TouchToFillPaymentMethodControllerImpl&) = delete;
  TouchToFillPaymentMethodControllerImpl& operator=(
      const TouchToFillPaymentMethodControllerImpl&) = delete;
  ~TouchToFillPaymentMethodControllerImpl() override;

  // TouchToFillPaymentMethodController:
  bool ShowPaymentMethods(std::unique_ptr<TouchToFillPaymentMethodView> view,
                          base::WeakPtr<TouchToFillDelegate> delegate,
                          base::span<const Suggestion> suggestions) override;
  bool ShowIbans(std::unique_ptr<TouchToFillPaymentMethodView> view,
                 base::WeakPtr<TouchToFillDelegate> delegate,
                 base::span<const Iban> ibans_to_suggest) override;
  bool ShowLoyaltyCards(std::unique_ptr<TouchToFillPaymentMethodView> view,
                        base::WeakPtr<TouchToFillDelegate> delegate,
                        base::span<const LoyaltyCard> affiliated_loyalty_cards,
                        base::span<const LoyaltyCard> all_loyalty_cards,
                        bool first_time_usage) override;
  bool OnPurchaseAmountExtracted(
      base::span<const payments::BnplIssuerContext> bnpl_issuer_contexts,
      std::optional<int64_t> extracted_amount,
      bool is_amount_supported_by_any_issuer,
      const std::optional<std::string>& app_locale,
      base::OnceCallback<void(BnplIssuer)> selected_issuer_callback,
      base::OnceClosure cancel_callback) override;
  bool ShowProgressScreen(std::unique_ptr<TouchToFillPaymentMethodView> view,
                          base::OnceClosure cancel_callback) override;
  bool ShowBnplIssuers(
      base::span<const payments::BnplIssuerContext> bnpl_issuer_contexts,
      const std::string& app_locale,
      base::OnceCallback<void(BnplIssuer)> selected_issuer_callback,
      base::OnceClosure cancel_callback) override;
  bool ShowErrorScreen(std::unique_ptr<TouchToFillPaymentMethodView> view,
                       const std::u16string& title,
                       const std::u16string& description) override;
  bool ShowBnplIssuerTos(BnplTosModel bnpl_tos_model,
                         base::OnceClosure accept_callback,
                         base::OnceClosure cancel_callback) override;
  void Hide() override;
  void SetVisible(bool visible) override;

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // ContentAutofillDriverFactory::Observer:
  void OnContentAutofillDriverFactoryDestroyed(
      ContentAutofillDriverFactory& factory) override;
  void OnContentAutofillDriverCreated(ContentAutofillDriverFactory& factory,
                                      ContentAutofillDriver& driver) override;

  // TouchToFillPaymentMethodViewController:
  void OnDismissed(JNIEnv* env,
                   bool dismissed_by_user,
                   bool should_reshow) override;
  void ScanCreditCard(JNIEnv* env) override;
  void ShowPaymentMethodSettings(JNIEnv* env) override;
  void CreditCardSuggestionSelected(JNIEnv* env,
                                    const std::string& unique_id,
                                    bool is_virtual) override;
  void BnplSuggestionSelected(JNIEnv* env,
                              std::optional<int64_t> extracted_amount) override;
  void LocalIbanSuggestionSelected(JNIEnv* env,
                                   const std::string& guid) override;
  void ServerIbanSuggestionSelected(JNIEnv* env, long instrument_id) override;
  void LoyaltyCardSuggestionSelected(JNIEnv* env,
                                     const LoyaltyCard& loyalty_card) override;
  void OnBnplIssuerSuggestionSelected(JNIEnv* env,
                                      const std::string& issuer_id) override;
  void OnBnplTosAccepted(JNIEnv* env) override;
  int GetJavaResourceId(int native_resource_id) const override;
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() override;
  void ResetJavaObject();

  TouchToFillKeyboardSuppressor& keyboard_suppressor_for_test() {
    return keyboard_suppressor_;
  }

 private:
  // Observes creation of ContentAutofillDrivers to inject a
  // TouchToFillDelegateAndroidImpl into the BrowserAutofillManager.
  base::ScopedObservation<ContentAutofillDriverFactory,
                          ContentAutofillDriverFactory::Observer>
      driver_factory_observation_{this};
  // Delegate for the surface being shown.
  base::WeakPtr<TouchToFillDelegate> delegate_;
  // View that displays the surface, owned by `this`.
  std::unique_ptr<TouchToFillPaymentMethodView> view_;
  // The corresponding Java TouchToFillPaymentMethodControllerBridge.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
  // Suppresses the keyboard between
  // AutofillManager::Observer::On{Before,After}AskForValuesToFill() events if
  // TTF may be shown.
  TouchToFillKeyboardSuppressor keyboard_suppressor_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_PAYMENT_METHOD_CONTROLLER_IMPL_H_
