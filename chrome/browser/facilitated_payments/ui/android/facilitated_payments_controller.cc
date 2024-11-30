// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/facilitated_payments/ui/android/facilitated_payments_controller.h"

#include <memory>
#include <utility>

#include "base/android/jni_android.h"
#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "components/autofill/core/browser/data_model/bank_account.h"
#include "components/autofill/core/browser/data_model/ewallet.h"
#include "components/facilitated_payments/core/ui_utils/facilitated_payments_ui_utils.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/facilitated_payments/ui/android/internal/jni/FacilitatedPaymentsPaymentMethodsControllerBridge_jni.h"

namespace {
const int64_t kFakeInstrumentId = -1L;
}  // namespace

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
    base::OnceCallback<void(bool, int64_t)> on_user_decision_callback) {
  // Abort if there are no bank accounts.
  if (bank_account_suggestions.empty()) {
    return;
  }

  view_->RequestShowContent(std::move(bank_account_suggestions));
  on_user_decision_callback_ = std::move(on_user_decision_callback);
}

void FacilitatedPaymentsController::ShowForEwallet(
    base::span<const autofill::Ewallet> ewallet_suggestions,
    base::OnceCallback<void(bool, int64_t)> on_user_decision_callback) {
  // Abort if there are no eWallets.
  if (ewallet_suggestions.empty()) {
    return;
  }

  view_->RequestShowContentForEwallet(std::move(ewallet_suggestions));
  on_user_decision_callback_ = std::move(on_user_decision_callback);
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

// TODO: crbug.com/375089558 - Deprecate once Java side is able to call
// OnUiEvent.
void FacilitatedPaymentsController::OnDismissed(JNIEnv* env) {
  ClearJavaViewComponents();

  if (on_user_decision_callback_) {
    std::move(on_user_decision_callback_)
        .Run(/*is_fop_selected=*/false, kFakeInstrumentId);
  }
}

void FacilitatedPaymentsController::OnBankAccountSelected(JNIEnv* env,
                                                          jlong instrument_id) {
  if (on_user_decision_callback_) {
    std::move(on_user_decision_callback_).Run(true, instrument_id);
  }
}

void FacilitatedPaymentsController::OnEwalletSelected(JNIEnv* env,
                                                      jlong instrument_id) {
  if (on_user_decision_callback_) {
    std::move(on_user_decision_callback_)
        .Run(/*is_ewallet_selected=*/true, instrument_id);
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
