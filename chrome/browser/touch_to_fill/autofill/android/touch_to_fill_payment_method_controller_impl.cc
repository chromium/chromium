// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_payment_method_controller_impl.h"

#include <memory>
#include <optional>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_delegate_android_impl.h"
#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_payment_method_view.h"
#include "chrome/browser/ui/autofill/payments/android_bnpl_ui_delegate.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/data_model/valuables/android/loyalty_card_android.h"
#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/integrators/touch_to_fill/touch_to_fill_delegate.h"
#include "components/autofill/core/browser/payments/bnpl_util.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/ui/payments/bnpl_tos_controller.h"
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
                   GetDelegate(manager)->IntendsToShowTouchToFill(form, field);
          }),
          base::Seconds(1)) {
  driver_factory_observation_.Observe(
      &autofill_client->GetAutofillDriverFactory());
}

TouchToFillPaymentMethodControllerImpl::
    ~TouchToFillPaymentMethodControllerImpl() {
  ResetJavaObject();
}

bool TouchToFillPaymentMethodControllerImpl::ShowPaymentMethods(
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

  if (!view->ShowPaymentMethods(this, suggestions,
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

bool TouchToFillPaymentMethodControllerImpl::OnPurchaseAmountExtracted(
    base::span<const payments::BnplIssuerContext> bnpl_issuer_contexts,
    std::optional<int64_t> extracted_amount,
    bool is_amount_supported_by_any_issuer,
    const std::optional<std::string>& app_locale,
    base::OnceCallback<void(BnplIssuer)> selected_issuer_callback,
    base::OnceClosure cancel_callback) {
  if (!view_ || !view_->OnPurchaseAmountExtracted(
                    *this, bnpl_issuer_contexts, extracted_amount,
                    is_amount_supported_by_any_issuer, app_locale)) {
    return false;
  }
  if (delegate_) {
    delegate_->SetCancelCallback(std::move(cancel_callback));
    delegate_->SetSelectedIssuerCallback(std::move(selected_issuer_callback));
  }
  return true;
}

bool TouchToFillPaymentMethodControllerImpl::ShowProgressScreen(
    std::unique_ptr<TouchToFillPaymentMethodView> view,
    base::OnceClosure cancel_callback) {
  if (view) {
    // If there is a view already being shown, reset it and use the new provided
    // view.
    if (view_) {
      ResetJavaObject();
    }
    view_ = std::move(view);
  }

  if (!view_ || !view_->ShowProgressScreen(this)) {
    ResetJavaObject();
    return false;
  }

  if (delegate_) {
    delegate_->SetCancelCallback(std::move(cancel_callback));
  }

  return true;
}

bool TouchToFillPaymentMethodControllerImpl::ShowBnplIssuers(
    base::span<const payments::BnplIssuerContext> bnpl_issuer_contexts,
    const std::string& app_locale,
    base::OnceCallback<void(BnplIssuer)> selected_issuer_callback,
    base::OnceClosure cancel_callback) {
  if (!view_ ||
      !view_->ShowBnplIssuers(*this, bnpl_issuer_contexts, app_locale)) {
    ResetJavaObject();
    return false;
  }
  if (delegate_) {
    delegate_->SetCancelCallback(std::move(cancel_callback));
    delegate_->SetSelectedIssuerCallback(std::move(selected_issuer_callback));
  }
  return true;
}

bool TouchToFillPaymentMethodControllerImpl::ShowErrorScreen(
    std::unique_ptr<TouchToFillPaymentMethodView> view,
    const std::u16string& title,
    const std::u16string& description) {
  if (view) {
    // If there is a view already being shown, reset it and use the new provided
    // view.
    if (view_) {
      ResetJavaObject();
    }
    view_ = std::move(view);
  }

  if (!view_ || !view_->ShowErrorScreen(this, title, description)) {
    ResetJavaObject();
    return false;
  }

  return true;
}

bool TouchToFillPaymentMethodControllerImpl::ShowBnplIssuerTos(
    BnplTosModel bnpl_tos_model,
    base::OnceClosure accept_callback,
    base::OnceClosure cancel_callback) {
  if (!view_ ||
      !view_->ShowBnplIssuerTos(
          *this,
          payments::BnplIssuerTosDetail(
              bnpl_tos_model.issuer.issuer_id(),
              /*header_icon_id=*/
              payments::AndroidBnplUiDelegate::GetDuoBrandedIconForBnplIssuer(
                  bnpl_tos_model.issuer.issuer_id(),
                  /*is_dark_mode=*/false),
              /*header_icon_id_dark=*/
              payments::AndroidBnplUiDelegate::GetDuoBrandedIconForBnplIssuer(
                  bnpl_tos_model.issuer.issuer_id(),
                  /*is_dark_mode=*/true),
              /*is_linked_issuer=*/
              bnpl_tos_model.issuer.payment_instrument().has_value(),
              bnpl_tos_model.issuer.GetDisplayName(),
              bnpl_tos_model.legal_message_lines))) {
    ResetJavaObject();
    return false;
  }

  if (delegate_) {
    delegate_->SetCancelCallback(std::move(cancel_callback));
    delegate_->SetBnplTosAcceptCallback(std::move(accept_callback));
  }

  return true;
}

void TouchToFillPaymentMethodControllerImpl::Hide() {
  if (view_) {
    view_->Hide();
  }
}

void TouchToFillPaymentMethodControllerImpl::SetVisible(bool visible) {
  if (view_) {
    view_->SetVisible(visible);
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

void TouchToFillPaymentMethodControllerImpl::OnDismissed(JNIEnv* env,
                                                         bool dismissed_by_user,
                                                         bool should_reshow) {
  if (delegate_) {
    delegate_->OnDismissed(dismissed_by_user, should_reshow);
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
    const std::string& unique_id,
    bool is_virtual) {
  if (delegate_) {
    delegate_->CreditCardSuggestionSelected(unique_id, is_virtual);
  }
}

void TouchToFillPaymentMethodControllerImpl::BnplSuggestionSelected(
    JNIEnv* env,
    std::optional<int64_t> extracted_amount) {
  if (delegate_) {
    delegate_->BnplSuggestionSelected(extracted_amount);
  }
}

void TouchToFillPaymentMethodControllerImpl::LocalIbanSuggestionSelected(
    JNIEnv* env,
    const std::string& guid) {
  if (delegate_) {
    delegate_->IbanSuggestionSelected(Iban::Guid(guid));
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
    const LoyaltyCard& loyalty_card) {
  if (delegate_) {
    delegate_->LoyaltyCardSuggestionSelected(loyalty_card);
  }
}

void TouchToFillPaymentMethodControllerImpl::OnBnplIssuerSuggestionSelected(
    JNIEnv* env,
    const std::string& issuer_id) {
  if (delegate_) {
    delegate_->OnBnplIssuerSuggestionSelected(issuer_id);
  }
}

void TouchToFillPaymentMethodControllerImpl::OnBnplTosAccepted(JNIEnv* env) {
  if (delegate_) {
    delegate_->OnBnplTosAccepted();
  }
}

int TouchToFillPaymentMethodControllerImpl::GetJavaResourceId(
    int native_resource_id) const {
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

DEFINE_JNI(TouchToFillPaymentMethodControllerBridge)
