// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/facilitated_payments/ui/android/facilitated_payments_controller.h"

#include <memory>

#include "base/android/jni_android.h"

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
  if (java_object_) {
    payments::facilitated::
        Java_FacilitatedPaymentsPaymentMethodsControllerBridge_onNativeDestroyed(
            base::android::AttachCurrentThread(), java_object_);
  }
}

bool FacilitatedPaymentsController::IsInLandscapeMode() {
  return view_->IsInLandscapeMode();
}

bool FacilitatedPaymentsController::Show(
    base::span<const autofill::BankAccount> bank_account_suggestions,
    base::OnceCallback<void(bool, int64_t)> on_user_decision_callback) {
  // Abort if there are no bank accounts.
  if (bank_account_suggestions.empty()) {
    return false;
  }

  if (!view_->RequestShowContent(std::move(bank_account_suggestions))) {
    view_->OnDismissed();
    java_object_.Reset();
    return false;
  }

  on_user_decision_callback_ = std::move(on_user_decision_callback);
  return true;
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

void FacilitatedPaymentsController::OnDismissed(JNIEnv* env) {
  view_->OnDismissed();
  java_object_.Reset();

  if (on_user_decision_callback_) {
    std::move(on_user_decision_callback_).Run(false, kFakeInstrumentId);
  }
}

void FacilitatedPaymentsController::OnBankAccountSelected(JNIEnv* env,
                                                          jlong instrument_id) {
  if (on_user_decision_callback_) {
    std::move(on_user_decision_callback_).Run(true, instrument_id);
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
