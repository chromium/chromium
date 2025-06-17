// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_payment_method_controller_impl.h"

#include <memory>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_delegate_android_impl.h"
#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_payment_method_view.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/integrators/touch_to_fill/touch_to_fill_delegate.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "content/public/browser/navigation_handle.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/TouchToFillPaymentMethodControllerBridge_jni.h"

namespace autofill {

namespace {
TouchToFillDelegateAndroidImpl* GetDelegate(AutofillManager& manager) {
  auto& bam = static_cast<BrowserAutofillManager&>(manager);
  return static_cast<TouchToFillDelegateAndroidImpl*>(
      bam.touch_to_fill_delegate());
}
}  // namespace

TouchToFillPaymentMethodControllerImpl::TouchToFillPaymentMethodControllerImpl(
    ContentAutofillClient* autofill_client)
    : content::WebContentsObserver(&autofill_client->GetWebContents()),
      keyboard_suppressor_(
          autofill_client,
          base::BindRepeating([](AutofillManager& manager) {
            return GetDelegate(manager) &&
                   GetDelegate(manager)->IsShowingTouchToFill();
          }),
          base::BindRepeating([](AutofillManager& manager,
                                 FormGlobalId form,
                                 FieldGlobalId field,
                                 const FormData& form_data) {
            return GetDelegate(manager) &&
                   GetDelegate(manager)->IntendsToShowTouchToFill(form, field,
                                                                  form_data);
          }),
          base::Seconds(1)) {
  driver_factory_observation_.Observe(
      &autofill_client->GetAutofillDriverFactory());
}

TouchToFillPaymentMethodControllerImpl::
    ~TouchToFillPaymentMethodControllerImpl() {
  ResetJavaObject();
}

bool TouchToFillPaymentMethodControllerImpl::ShowCreditCards(
    std::unique_ptr<TouchToFillPaymentMethodView> view,
    base::WeakPtr<TouchToFillDelegate> delegate,
    base::span<const Suggestion> suggestions) {
  if (!keyboard_suppressor_.is_suppressing()) {
    return false;
  }

  // Abort if TTF surface is already shown.
  if (view_) {
    return false;
  }

  if (!view->ShowCreditCards(this, suggestions,
                             delegate->ShouldShowScanCreditCard())) {
    ResetJavaObject();
    return false;
  }

  view_ = std::move(view);
  delegate_ = std::move(delegate);
  return true;
}

bool TouchToFillPaymentMethodControllerImpl::ShowIbans(
    std::unique_ptr<TouchToFillPaymentMethodView> view,
    base::WeakPtr<TouchToFillDelegate> delegate,
    base::span<const Iban> ibans_to_suggest) {
  if (!keyboard_suppressor_.is_suppressing()) {
    return false;
  }

  // Abort if TTF surface is already shown.
  if (view_) {
    return false;
  }

  if (!view->ShowIbans(this, ibans_to_suggest)) {
    ResetJavaObject();
    return false;
  }

  view_ = std::move(view);
  delegate_ = std::move(delegate);
  return true;
}

bool TouchToFillPaymentMethodControllerImpl::ShowLoyaltyCards(
    std::unique_ptr<TouchToFillPaymentMethodView> view,
    base::WeakPtr<TouchToFillDelegate> delegate,
    base::span<const LoyaltyCard> affiliated_loyalty_cards,
    base::span<const LoyaltyCard> all_loyalty_cards,
    bool first_time_usage) {
  // TODO(crbug.com/404437211): Unify `ShowX()` methods to avoid code
  // duplication.
  if (!keyboard_suppressor_.is_suppressing()) {
    return false;
  }

  // Abort if TTF surface is already shown.
  if (view_) {
    return false;
  }

  if (!view->ShowLoyaltyCards(this, affiliated_loyalty_cards, all_loyalty_cards,
                              first_time_usage)) {
    ResetJavaObject();
    return false;
  }

  view_ = std::move(view);
  delegate_ = std::move(delegate);
  return true;
}

void TouchToFillPaymentMethodControllerImpl::Hide() {
  if (view_) {
    view_->Hide();
  }
}

void TouchToFillPaymentMethodControllerImpl::WebContentsDestroyed() {
  Hide();
}

void TouchToFillPaymentMethodControllerImpl::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted() ||
      navigation_handle->IsInPrerenderedMainFrame() ||
      (!navigation_handle->IsInMainFrame() &&
       !navigation_handle->HasSubframeNavigationEntryCommitted())) {
    return;
  }
  Hide();
}

void TouchToFillPaymentMethodControllerImpl::
    OnContentAutofillDriverFactoryDestroyed(
        ContentAutofillDriverFactory& factory) {
  driver_factory_observation_.Reset();
}

void TouchToFillPaymentMethodControllerImpl::OnContentAutofillDriverCreated(
    ContentAutofillDriverFactory& factory,
    ContentAutofillDriver& driver) {
  auto& manager =
      static_cast<BrowserAutofillManager&>(driver.GetAutofillManager());
  manager.set_touch_to_fill_delegate(
      std::make_unique<TouchToFillDelegateAndroidImpl>(&manager));
}

void TouchToFillPaymentMethodControllerImpl::OnDismissed(
    JNIEnv* env,
    bool dismissed_by_user) {
  if (delegate_) {
    delegate_->OnDismissed(dismissed_by_user);
  }
  view_.reset();
  delegate_.reset();
  ResetJavaObject();
  keyboard_suppressor_.Unsuppress();
}

void TouchToFillPaymentMethodControllerImpl::ScanCreditCard(JNIEnv* env) {
  if (delegate_) {
    delegate_->ScanCreditCard();
  }
}

void TouchToFillPaymentMethodControllerImpl::ShowPaymentMethodSettings(
    JNIEnv* env) {
  if (delegate_) {
    delegate_->ShowPaymentMethodSettings();
  }
}

void TouchToFillPaymentMethodControllerImpl::CreditCardSuggestionSelected(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& unique_id,
    bool is_virtual) {
  if (delegate_) {
    delegate_->CreditCardSuggestionSelected(
        base::android::ConvertJavaStringToUTF8(env, unique_id), is_virtual);
  }
}

void TouchToFillPaymentMethodControllerImpl::LocalIbanSuggestionSelected(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& guid) {
  if (delegate_) {
    delegate_->IbanSuggestionSelected(
        Iban::Guid((*env).GetStringUTFChars(guid, nullptr)));
  }
}

void TouchToFillPaymentMethodControllerImpl::ServerIbanSuggestionSelected(
    JNIEnv* env,
    long instrument_id) {
  if (delegate_) {
    delegate_->IbanSuggestionSelected(Iban::InstrumentId(instrument_id));
  }
}

void TouchToFillPaymentMethodControllerImpl::LoyaltyCardSuggestionSelected(
    JNIEnv* env,
    const std::string& loyalty_card_number) {
  if (delegate_) {
    delegate_->LoyaltyCardSuggestionSelected(loyalty_card_number);
  }
}

int TouchToFillPaymentMethodControllerImpl::GetJavaResourceId(
    int native_resource_id) {
  return ResourceMapper::MapToJavaDrawableId(native_resource_id);
}

base::android::ScopedJavaLocalRef<jobject>
TouchToFillPaymentMethodControllerImpl::GetJavaObject() {
  if (!java_object_) {
    java_object_ = Java_TouchToFillPaymentMethodControllerBridge_create(
        base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this),
        web_contents()->GetTopLevelNativeWindow()->GetJavaObject());
  }
  return base::android::ScopedJavaLocalRef<jobject>(java_object_);
}

void TouchToFillPaymentMethodControllerImpl::ResetJavaObject() {
  if (java_object_) {
    Java_TouchToFillPaymentMethodControllerBridge_onNativeDestroyed(
        base::android::AttachCurrentThread(), java_object_);
  }
  java_object_.Reset();
}

}  // namespace autofill
