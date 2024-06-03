// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/facilitated_payments/ui/android/facilitated_payments_bottom_sheet_bridge.h"

#include "base/android/jni_android.h"
#include "chrome/browser/autofill/android/personal_data_manager_android.h"
#include "chrome/browser/facilitated_payments/ui/android/facilitated_payments_controller.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/facilitated_payments/ui/android/internal/jni/FacilitatedPaymentsPaymentMethodsViewBridge_jni.h"

namespace payments::facilitated {

FacilitatedPaymentsBottomSheetBridge::FacilitatedPaymentsBottomSheetBridge() =
    default;

FacilitatedPaymentsBottomSheetBridge::~FacilitatedPaymentsBottomSheetBridge() =
    default;

bool FacilitatedPaymentsBottomSheetBridge::RequestShowContent(
    base::span<const autofill::BankAccount> bank_account_suggestions,
    FacilitatedPaymentsController* controller,
    content::WebContents* web_contents) {
  if (java_bridge_) {
    return false;  // Already shown.
  }

  if (!web_contents->GetNativeView() ||
      !web_contents->GetNativeView()->GetWindowAndroid()) {
    return false;  // No window attached (yet or anymore).
  }

  DCHECK(controller);
  base::android::ScopedJavaLocalRef<jobject> java_controller =
      controller->GetJavaObject();
  if (!java_controller) {
    return false;
  }

  JNIEnv* env = base::android::AttachCurrentThread();

  java_bridge_.Reset(Java_FacilitatedPaymentsPaymentMethodsViewBridge_create(
      env, java_controller,
      web_contents->GetTopLevelNativeWindow()->GetJavaObject()));
  if (!java_bridge_) {
    return false;
  }

  std::vector<base::android::ScopedJavaLocalRef<jobject>> bank_accounts_array;
  bank_accounts_array.reserve(bank_account_suggestions.size());
  for (const autofill::BankAccount& bank_account : bank_account_suggestions) {
    bank_accounts_array.push_back(
        autofill::PersonalDataManagerAndroid::CreateJavaBankAccountFromNative(
            env, bank_account));
  }
  return Java_FacilitatedPaymentsPaymentMethodsViewBridge_requestShowContent(
      env, java_bridge_, std::move(bank_accounts_array));
}

}  // namespace payments::facilitated
