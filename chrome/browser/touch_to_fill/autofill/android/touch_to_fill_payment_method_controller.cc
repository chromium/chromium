// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_payment_method_controller.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_delegate_android_impl.h"
#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_payment_method_view.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/touch_to_fill_delegate.h"
#include "content/public/browser/navigation_handle.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/touch_to_fill/autofill/android/internal/jni/TouchToFillPaymentMethodControllerBridge_jni.h"

namespace autofill {

namespace {
TouchToFillDelegateAndroidImpl* GetDelegate(AutofillManager& manager) {
  auto& bam = static_cast<BrowserAutofillManager&>(manager);
  return static_cast<TouchToFillDelegateAndroidImpl*>(
      bam.touch_to_fill_delegate());
}
}  // namespace

TouchToFillPaymentMethodController::TouchToFillPaymentMethodController(
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

TouchToFillPaymentMethodController::~TouchToFillPaymentMethodController() {
  ResetJavaObject();
}

void TouchToFillPaymentMethodController::WebContentsDestroyed() {
  Hide();
}

void TouchToFillPaymentMethodController::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted() ||
      navigation_handle->IsInPrerenderedMainFrame() ||
      (!navigation_handle->IsInMainFrame() &&
       !navigation_handle->HasSubframeNavigationEntryCommitted())) {
    return;
  }
  Hide();
}

void TouchToFillPaymentMethodController::OnContentAutofillDriverFactoryDestroyed(
    ContentAutofillDriverFactory& factory) {
  driver_factory_observation_.Reset();
}

void TouchToFillPaymentMethodController::OnContentAutofillDriverCreated(
    ContentAutofillDriverFactory& factory,
    ContentAutofillDriver& driver) {
  auto& manager =
      static_cast<BrowserAutofillManager&>(driver.GetAutofillManager());
  manager.set_touch_to_fill_delegate(
      std::make_unique<TouchToFillDelegateAndroidImpl>(&manager));
}

bool TouchToFillPaymentMethodController::Show(
    std::unique_ptr<TouchToFillPaymentMethodView> view,
    base::WeakPtr<TouchToFillDelegate> delegate,
    base::span<const CreditCard> cards_to_suggest,
    base::span<const Suggestion> suggestions) {
  if (!keyboard_suppressor_.is_suppressing()) {
    return false;
  }

  // Abort if TTF surface is already shown.
  if (view_)
    return false;

  if (!view->Show(this, cards_to_suggest, suggestions,
                  delegate->ShouldShowScanCreditCard())) {
    ResetJavaObject();
    return false;
  }

  view_ = std::move(view);
  delegate_ = std::move(delegate);
  return true;
}

bool TouchToFillPaymentMethodController::Show(
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

  if (!view->Show(this, ibans_to_suggest)) {
    ResetJavaObject();
    return false;
  }

  view_ = std::move(view);
  delegate_ = std::move(delegate);
  return true;
}

void TouchToFillPaymentMethodController::Hide() {
  if (view_)
    view_->Hide();
}

void TouchToFillPaymentMethodController::OnDismissed(JNIEnv* env,
                                                  bool dismissed_by_user) {
  if (delegate_) {
    delegate_->OnDismissed(dismissed_by_user);
  }
  view_.reset();
  delegate_.reset();
  ResetJavaObject();
  keyboard_suppressor_.Unsuppress();
}

void TouchToFillPaymentMethodController::ScanCreditCard(JNIEnv* env) {
  if (delegate_) {
    delegate_->ScanCreditCard();
  }
}

void TouchToFillPaymentMethodController::ShowPaymentMethodSettings(JNIEnv* env) {
  if (delegate_) {
    delegate_->ShowPaymentMethodSettings();
  }
}

void TouchToFillPaymentMethodController::CreditCardSuggestionSelected(
    JNIEnv* env,
    base::android::JavaParamRef<jstring> unique_id,
    bool is_virtual) {
  if (delegate_) {
    delegate_->CreditCardSuggestionSelected(
        base::android::ConvertJavaStringToUTF8(env, unique_id), is_virtual);
  }
}

void TouchToFillPaymentMethodController::LocalIbanSuggestionSelected(
    JNIEnv* env,
    base::android::JavaParamRef<jstring> guid) {
  if (delegate_) {
    delegate_->IbanSuggestionSelected(
        Iban::Guid((*env).GetStringUTFChars(guid, nullptr)));
  }
}

void TouchToFillPaymentMethodController::ServerIbanSuggestionSelected(
    JNIEnv* env,
    long instrument_id) {
  if (delegate_) {
    delegate_->IbanSuggestionSelected(Iban::InstrumentId(instrument_id));
  }
}

base::android::ScopedJavaLocalRef<jobject>
TouchToFillPaymentMethodController::GetJavaObject() {
  if (!java_object_) {
    java_object_ = Java_TouchToFillPaymentMethodControllerBridge_create(
        base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this));
  }
  return base::android::ScopedJavaLocalRef<jobject>(java_object_);
}

void TouchToFillPaymentMethodController::ResetJavaObject() {
  if (java_object_) {
    Java_TouchToFillPaymentMethodControllerBridge_onNativeDestroyed(
        base::android::AttachCurrentThread(), java_object_);
  }
  java_object_.Reset();
}

}  // namespace autofill
