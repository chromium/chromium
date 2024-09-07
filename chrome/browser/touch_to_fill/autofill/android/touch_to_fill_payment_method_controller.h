// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_PAYMENT_METHOD_CONTROLLER_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_PAYMENT_METHOD_CONTROLLER_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_payment_method_view_controller.h"
#include "components/autofill/android/touch_to_fill_keyboard_suppressor.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/ui/suggestion.h"

namespace autofill {

class ContentAutofillClient;
class CreditCard;
class Iban;
class TouchToFillDelegate;
class TouchToFillPaymentMethodView;

// Controller of the bottom sheet surface for filling credit card or IBAN data on
// Android. It is responsible for showing the view and handling user
// interactions. While the surface is shown, stores its Java counterpart in
// `java_object_`.
class TouchToFillPaymentMethodController
    : public TouchToFillPaymentMethodViewController,
      public ContentAutofillDriverFactory::Observer,
      public content::WebContentsObserver {
 public:
  explicit TouchToFillPaymentMethodController(
      ContentAutofillClient* autofill_client);
  TouchToFillPaymentMethodController(const TouchToFillPaymentMethodController&) =
      delete;
  TouchToFillPaymentMethodController& operator=(
      const TouchToFillPaymentMethodController&) = delete;
  ~TouchToFillPaymentMethodController() override;

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // ContentAutofillDriverFactory::Observer:
  void OnContentAutofillDriverFactoryDestroyed(
      ContentAutofillDriverFactory& factory) override;
  void OnContentAutofillDriverCreated(ContentAutofillDriverFactory& factory,
                                      ContentAutofillDriver& driver) override;

  // Shows the Touch To Fill `view`. `delegate` will provide the fillable credit
  // cards and be notified of the user's decision. `suggestions` are generated
  // using the `cards_to_suggest` data and include fields such as `main_text`,
  // `minor_text`, and `apply_deactivated_style`. The `apply_deactivated_style`
  // field determines which card suggestions should be disabled and grayed out
  // for the current merchant. Returns whether the surface was successfully
  // shown.
  bool Show(std::unique_ptr<TouchToFillPaymentMethodView> view,
            base::WeakPtr<TouchToFillDelegate> delegate,
            base::span<const CreditCard> cards_to_suggest,
            base::span<const Suggestion> suggestions);

  // Shows the Touch To Fill `view`. `delegate` will provide the fillable IBANs
  // and be notified of the user's decision. Returns whether the surface was
  // successfully shown.
  bool Show(std::unique_ptr<TouchToFillPaymentMethodView> view,
            base::WeakPtr<TouchToFillDelegate> delegate,
            base::span<const Iban> ibans_to_suggest);

  // Hides the surface if it is currently shown.
  void Hide();

  // TouchToFillPaymentMethodViewController:
  void OnDismissed(JNIEnv* env, bool dismissed_by_user) override;
  void ScanCreditCard(JNIEnv* env) override;
  void ShowPaymentMethodSettings(JNIEnv* env) override;
  void CreditCardSuggestionSelected(
      JNIEnv* env,
      base::android::JavaParamRef<jstring> unique_id,
      bool is_virtual) override;
  void LocalIbanSuggestionSelected(
      JNIEnv* env,
      base::android::JavaParamRef<jstring> guid) override;
  void ServerIbanSuggestionSelected(JNIEnv* env, long instrument_id) override;

  TouchToFillKeyboardSuppressor& keyboard_suppressor_for_test() {
    return keyboard_suppressor_;
  }

 private:
  // Gets or creates the Java counterpart.
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() override;
  void ResetJavaObject();

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

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_PAYMENT_METHOD_CONTROLLER_H_
