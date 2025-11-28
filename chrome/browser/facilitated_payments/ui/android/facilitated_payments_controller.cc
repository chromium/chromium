// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/facilitated_payments/ui/android/facilitated_payments_controller.h"

#include <memory>
#include <string_view>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "components/autofill/core/browser/data_model/payments/bank_account.h"
#include "components/autofill/core/browser/data_model/payments/ewallet.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_app_info_list.h"
#include "components/facilitated_payments/core/browser/payment_link_manager.h"
#include "components/facilitated_payments/core/utils/facilitated_payments_ui_utils.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/facilitated_payments/ui/android/internal/jni/FacilitatedPaymentsPaymentMethodsControllerBridge_jni.h"

FacilitatedPaymentsController::FacilitatedPaymentsController(
    content::WebContents* web_contents)
    : view_(std::make_unique<
            payments::facilitated::FacilitatedPaymentsBottomSheetBridge>(
          web_contents,
          this)) {}

FacilitatedPaymentsController::~FacilitatedPaymentsController() {
  ClearJavaViewComponents();
}

bool FacilitatedPaymentsController::IsInLandscapeMode() {
  return view_->IsInLandscapeMode();
}

void FacilitatedPaymentsController::Show(
    base::span<const autofill::BankAccount> bank_account_suggestions,
    base::OnceCallback<void(int64_t)> on_payment_account_selected) {
  // Abort if there are no bank accounts.
  if (bank_account_suggestions.empty()) {
    return;
  }

  view_->RequestShowContent(std::move(bank_account_suggestions));
  on_payment_account_selected_ = std::move(on_payment_account_selected);
}

void FacilitatedPaymentsController::ShowForPaymentLink(
    base::span<const autofill::Ewallet> ewallet_suggestions,
    std::unique_ptr<payments::facilitated::FacilitatedPaymentsAppInfoList>
        app_suggestions,
    base::OnceCallback<void(payments::facilitated::SelectedFopData)>
        on_fop_selected) {
  // Abort if there are no eWallets and no payment apps.
  if (ewallet_suggestions.empty() &&
      (app_suggestions == nullptr || app_suggestions->Size() == 0)) {
    return;
  }

  view_->RequestShowContentForPaymentLink(std::move(ewallet_suggestions),
                                          std::move(app_suggestions));
  on_fop_selected_ = std::move(on_fop_selected);
}

void FacilitatedPaymentsController::ShowProgressScreen() {
  view_->ShowProgressScreen();
}

void FacilitatedPaymentsController::ShowErrorScreen() {
  view_->ShowErrorScreen();
}

void FacilitatedPaymentsController::Dismiss() {
  view_->Dismiss();
}

void FacilitatedPaymentsController::SetUiEventListener(
    base::RepeatingCallback<void(payments::facilitated::UiEvent)>
        ui_event_listener) {
  ui_event_listener_ = std::move(ui_event_listener);
}

void FacilitatedPaymentsController::OnUiEvent(JNIEnv* env, jint event) {
  CHECK(event >= static_cast<jint>(
                     payments::facilitated::UiEvent::kNewScreenShown) &&
        event <= static_cast<jint>(payments::facilitated::UiEvent::kMaxValue))
      << "Invalid payments::facilitated::UiEvent value: " << event;

  // `payments::facilitated::UiEvent` is synced to the Java side.
  payments::facilitated::UiEvent ui_event =
      static_cast<payments::facilitated::UiEvent>(event);
  switch (ui_event) {
    case payments::facilitated::UiEvent::kScreenCouldNotBeShown:
    case payments::facilitated::UiEvent::kScreenClosedNotByUser:
    case payments::facilitated::UiEvent::kScreenClosedByUser:
      ClearJavaViewComponents();
      break;
    case payments::facilitated::UiEvent::kNewScreenShown:
      break;
  }
  if (ui_event_listener_) {
    ui_event_listener_.Run(ui_event);
  }
}

void FacilitatedPaymentsController::OnBankAccountSelected(JNIEnv* env,
                                                          jlong instrument_id) {
  if (on_payment_account_selected_) {
    std::move(on_payment_account_selected_).Run(instrument_id);
  }
}

void FacilitatedPaymentsController::OnEwalletSelected(JNIEnv* env,
                                                      jlong instrument_id) {
  if (on_fop_selected_) {
    std::move(on_fop_selected_)
        .Run(payments::facilitated::SelectedFopData(instrument_id));
  }
}

void FacilitatedPaymentsController::OnPaymentAppSelected(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& j_package_name,
    const base::android::JavaParamRef<jstring>& j_activity_name) {
  if (on_fop_selected_) {
    std::move(on_fop_selected_)
        .Run(payments::facilitated::SelectedFopData(
            base::android::ConvertJavaStringToUTF8(env, j_package_name),
            base::android::ConvertJavaStringToUTF8(env, j_activity_name)));
  }
}

void FacilitatedPaymentsController::ShowPixAccountLinkingPrompt(
    base::OnceCallback<void()> on_accepted,
    base::OnceCallback<void()> on_declined) {
  on_pix_account_linking_prompt_accepted_ = std::move(on_accepted);
  on_pix_account_linking_prompt_declined_ = std::move(on_declined);
  view_->ShowPixAccountLinkingPrompt();
}

void FacilitatedPaymentsController::OnPixAccountLinkingPromptAccepted(
    JNIEnv* env) {
  if (on_pix_account_linking_prompt_accepted_) {
    std::move(on_pix_account_linking_prompt_accepted_).Run();
  }
}

void FacilitatedPaymentsController::OnPixAccountLinkingPromptDeclined(
    JNIEnv* env) {
  if (on_pix_account_linking_prompt_declined_) {
    std::move(on_pix_account_linking_prompt_declined_).Run();
  }
}

base::android::ScopedJavaLocalRef<jobject>
FacilitatedPaymentsController::GetJavaObject() {
  if (!java_object_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    java_object_ = payments::facilitated::
        Java_FacilitatedPaymentsPaymentMethodsControllerBridge_create(
            env, reinterpret_cast<intptr_t>(this));
  }
  return base::android::ScopedJavaLocalRef<jobject>(java_object_);
}

void FacilitatedPaymentsController::SetViewForTesting(
    std::unique_ptr<payments::facilitated::FacilitatedPaymentsBottomSheetBridge>
        view) {
  view_ = std::move(view);
}

void FacilitatedPaymentsController::ClearJavaViewComponents() {
  view_->OnDismissed();
  if (java_object_) {
    payments::facilitated::
        Java_FacilitatedPaymentsPaymentMethodsControllerBridge_onNativeDestroyed(
            base::android::AttachCurrentThread(), java_object_);
  }
  java_object_.Reset();
}

DEFINE_JNI(FacilitatedPaymentsPaymentMethodsControllerBridge)
