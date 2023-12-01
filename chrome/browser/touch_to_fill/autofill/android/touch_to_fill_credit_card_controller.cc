// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_credit_card_controller.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/touch_to_fill/autofill/android/internal/jni/TouchToFillCreditCardControllerBridge_jni.h"
#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_credit_card_view.h"
#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_delegate_android_impl.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/ui/touch_to_fill_delegate.h"

namespace autofill {

namespace {
TouchToFillDelegateAndroidImpl* GetDelegate(AutofillManager& manager) {
  auto& bam = static_cast<BrowserAutofillManager&>(manager);
  return static_cast<TouchToFillDelegateAndroidImpl*>(
      bam.touch_to_fill_delegate());
}
}  // namespace

TouchToFillCreditCardController::TouchToFillCreditCardController(
    ContentAutofillClient* autofill_client)
    : keyboard_suppressor_(
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
      autofill_client->GetAutofillDriverFactory());
}

TouchToFillCreditCardController::~TouchToFillCreditCardController() {
  if (java_object_) {
    Java_TouchToFillCreditCardControllerBridge_onNativeDestroyed(
        base::android::AttachCurrentThread(), java_object_);
  }
}

void TouchToFillCreditCardController::OnContentAutofillDriverFactoryDestroyed(
    ContentAutofillDriverFactory& factory) {
  driver_factory_observation_.Reset();
}

void TouchToFillCreditCardController::OnContentAutofillDriverCreated(
    ContentAutofillDriverFactory& factory,
    ContentAutofillDriver& driver) {
  auto& manager =
      static_cast<BrowserAutofillManager&>(driver.GetAutofillManager());
  manager.set_touch_to_fill_delegate(
      std::make_unique<TouchToFillDelegateAndroidImpl>(&manager));
}

bool TouchToFillCreditCardController::Show(
    std::unique_ptr<TouchToFillCreditCardView> view,
    base::WeakPtr<TouchToFillDelegate> delegate,
    base::span<const CreditCard> cards_to_suggest) {
  if (!keyboard_suppressor_.is_suppressing()) {
    return false;
  }

  // Abort if TTF surface is already shown.
  if (view_)
    return false;

  if (!view->Show(this, std::move(cards_to_suggest),
                  delegate->ShouldShowScanCreditCard())) {
    java_object_.Reset();
    return false;
  }

  view_ = std::move(view);
  delegate_ = std::move(delegate);
  return true;
}

void TouchToFillCreditCardController::Hide() {
  if (view_)
    view_->Hide();
}

void TouchToFillCreditCardController::OnDismissed(JNIEnv* env,
                                                  bool dismissed_by_user) {
  if (delegate_) {
    delegate_->OnDismissed(dismissed_by_user);
  }
  view_.reset();
  delegate_.reset();
  java_object_.Reset();
  keyboard_suppressor_.Unsuppress();
}

void TouchToFillCreditCardController::ScanCreditCard(JNIEnv* env) {
  delegate_->ScanCreditCard();
}

void TouchToFillCreditCardController::ShowCreditCardSettings(JNIEnv* env) {
  delegate_->ShowCreditCardSettings();
}

void TouchToFillCreditCardController::SuggestionSelected(
    JNIEnv* env,
    base::android::JavaParamRef<jstring> unique_id,
    bool is_virtual) {
  delegate_->SuggestionSelected(
      base::android::ConvertJavaStringToUTF8(env, unique_id), is_virtual);
}

base::android::ScopedJavaLocalRef<jobject>
TouchToFillCreditCardController::GetJavaObject() {
  if (!java_object_) {
    java_object_ = Java_TouchToFillCreditCardControllerBridge_create(
        base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this));
  }
  return base::android::ScopedJavaLocalRef<jobject>(java_object_);
}

}  // namespace autofill
