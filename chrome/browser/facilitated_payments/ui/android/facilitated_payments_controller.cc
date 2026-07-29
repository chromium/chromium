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
#include "components/facilitated_payments/core/metrics/facilitated_payments_metrics.h"
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

void FacilitatedPaymentsController::OnUiEvent(JNIEnv* env, int32_t event) {
  CHECK(event >= static_cast<int32_t>(
                     payments::facilitated::UiEvent::kNewScreenShown) &&
        event <=
            static_cast<int32_t>(payments::facilitated::UiEvent::kMaxValue))
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

void FacilitatedPaymentsController::OnBankAccountSelected(
    JNIEnv* env,
    int64_t instrument_id) {
  if (on_payment_account_selected_) {
    std::move(on_payment_account_selected_).Run(instrument_id);
  }
}

void FacilitatedPaymentsController::OnEwalletSelected(JNIEnv* env,
                                                      int64_t instrument_id) {
  if (on_fop_selected_) {
    std::move(on_fop_selected_)
        .Run(payments::facilitated::SelectedFopData(instrument_id));
  }
}

void FacilitatedPaymentsController::OnPaymentAppSelected(
    JNIEnv* env,
    const base::android::JavaRef<jstring>& j_package_name,
    const base::android::JavaRef<jstring>& j_activity_name) {
  if (on_fop_selected_) {
    std::move(on_fop_selected_)
        .Run(payments::facilitated::SelectedFopData(
            base::android::ConvertJavaStringToUTF8(env, j_package_name),
            base::android::ConvertJavaStringToUTF8(env, j_activity_name)));
  }
}

void FacilitatedPaymentsController::ShowPixAccountLinkingPrompt(
    int strike_count,
    base::OnceCallback<void()> on_accepted,
    base::OnceCallback<void()> on_declined) {
  on_pix_account_linking_prompt_accepted_ = std::move(on_accepted);
  on_pix_account_linking_prompt_declined_ = std::move(on_declined);
  view_->ShowPixAccountLinkingPrompt(strike_count);
}

void FacilitatedPaymentsController::ShowPixAccountLinkingSuccessScreen() {
  view_->ShowPixAccountLinkingSuccessScreen();
}

void FacilitatedPaymentsController::ShowAccountLinkingPrompt(
    const payments::facilitated::AccountLinkingParams& params,
    base::OnceCallback<void()> on_accepted,
    base::OnceCallback<void()> on_declined,
    base::OnceCallback<void()> on_dismissed) {
  if (is_prompt_showing_) {
    payments::facilitated::LogAccountLinkingPromptFailedToShow(
        params.fop_type);
    return;
  }
  is_prompt_showing_ = true;
  account_linking_prompt_shown_time_ = base::TimeTicks::Now();
  on_accepted_callback_ = std::move(on_accepted);
  on_declined_callback_ = std::move(on_declined);
  on_dismissed_callback_ = std::move(on_dismissed);
  CHECK(view_);
  if (!view_->ShowAccountLinkingPrompt(params)) {
    payments::facilitated::LogAccountLinkingPromptFailedToShow(
        params.fop_type);
    DismissPrompt();
    return;
  }
}

void FacilitatedPaymentsController::ShowAccountLinkingFailureNotification(
    payments::facilitated::FacilitatedPaymentsType fop_type) {
  view_->ShowAccountLinkingFailureNotification(fop_type);
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

void FacilitatedPaymentsController::OnAccountLinkingPromptShown(JNIEnv * env,
                                                                int32_t type) {
  payments::facilitated::LogAccountLinkingPromptUserAction(
      static_cast<payments::facilitated::FacilitatedPaymentsType>(type),
      payments::facilitated::AccountLinkingPromptUserAction::kShown);
}

void FacilitatedPaymentsController::OnAccountLinkingPromptAction(
    JNIEnv * env, int32_t type, int32_t action) {
  // kShown is handled exclusively by OnAccountLinkingPromptShown, so we use >
  // rather than >= here.
  CHECK(
      action >
          static_cast<int32_t>(
              payments::facilitated::AccountLinkingPromptUserAction::kShown) &&
      action <=
          static_cast<int32_t>(
              payments::facilitated::AccountLinkingPromptUserAction::kMaxValue))
      << "Invalid payments::facilitated::AccountLinkingPromptUserAction value: "
      << action;

  if (!is_prompt_showing_) {
    return;
  }
  base::OnceClosure on_accepted = std::move(on_accepted_callback_);
  base::OnceClosure on_declined = std::move(on_declined_callback_);
  base::OnceClosure on_dismissed = std::move(on_dismissed_callback_);
  auto user_action =
      static_cast<payments::facilitated::AccountLinkingPromptUserAction>(
          action);

  payments::facilitated::LogAccountLinkingPromptUserAction(
      static_cast<payments::facilitated::FacilitatedPaymentsType>(type),
      user_action);
  payments::facilitated::LogAccountLinkingPromptInteractionDuration(
      static_cast<payments::facilitated::FacilitatedPaymentsType>(type),
      user_action, base::TimeTicks::Now() - account_linking_prompt_shown_time_);
  is_prompt_showing_ = false;

  switch (user_action) {
    case payments::facilitated::AccountLinkingPromptUserAction::kAccepted:
      if (on_accepted) {
        std::move(on_accepted).Run();
      }
      break;
    case payments::facilitated::AccountLinkingPromptUserAction::kDeclined:
      if (on_declined) {
        std::move(on_declined).Run();
      }
      break;
    case payments::facilitated::AccountLinkingPromptUserAction::kDismissed:
      if (on_dismissed) {
        std::move(on_dismissed).Run();
      }
      break;
    default:
      // Gracefully handle unexpected JNI inputs by acting as if dismissed.
      if (on_dismissed) {
        std::move(on_dismissed).Run();
      }
      break;
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

  DismissPrompt();
}

void FacilitatedPaymentsController::DismissPrompt() {
  if (!is_prompt_showing_) {
    return;
  }
  is_prompt_showing_ = false;

  base::OnceClosure dismissed_callback = std::move(on_dismissed_callback_);
  on_accepted_callback_.Reset();
  on_declined_callback_.Reset();
  // We use a local variable to ensure the object's internal state remains valid
  // even if the callback invocation destroys the object.
  if (dismissed_callback) {
    std::move(dismissed_callback).Run();
  }
}

DEFINE_JNI(FacilitatedPaymentsPaymentMethodsControllerBridge)
