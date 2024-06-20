// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/facilitated_payments/ui/android/facilitated_payments_controller.h"

#include <memory>

#include "base/android/jni_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/facilitated_payments/ui/android/internal/jni/FacilitatedPaymentsPaymentMethodsControllerBridge_jni.h"

FacilitatedPaymentsController::FacilitatedPaymentsController() = default;
FacilitatedPaymentsController::~FacilitatedPaymentsController() = default;

namespace {
const int64_t kFakeInstrumentId = -1L;
}  // namespace

bool FacilitatedPaymentsController::Show(
    content::WebContents* web_contents,
    std::unique_ptr<payments::facilitated::FacilitatedPaymentsBottomSheetBridge>
        view,
    base::span<const autofill::BankAccount> bank_account_suggestions,
    base::OnceCallback<void(bool, int64_t)> on_user_decision_callback) {
  // Abort if facilitated payments surface is already shown or no bank accounts.
  if (view_ || bank_account_suggestions.empty()) {
    return false;
  }

  if (!view->RequestShowContent(std::move(bank_account_suggestions), this,
                                web_contents)) {
    java_object_.Reset();
    return false;
  }

  view_ = std::move(view);
  on_user_decision_callback_ = std::move(on_user_decision_callback);
  return true;
}

void FacilitatedPaymentsController::OnDismissed(JNIEnv* env) {
  view_.reset();
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
    java_object_ = payments::facilitated::
        Java_FacilitatedPaymentsPaymentMethodsControllerBridge_create(
            base::android::AttachCurrentThread(),
            reinterpret_cast<intptr_t>(this));
  }
  return base::android::ScopedJavaLocalRef<jobject>(java_object_);
}
