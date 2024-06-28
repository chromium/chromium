// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/facilitated_payments/ui/android/facilitated_payments_bottom_sheet_bridge.h"

#include "base/android/jni_android.h"
#include "chrome/browser/autofill/android/personal_data_manager_android.h"
#include "chrome/browser/facilitated_payments/ui/android/facilitated_payments_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/facilitated_payments/ui/android/internal/jni/FacilitatedPaymentsPaymentMethodsViewBridge_jni.h"

namespace payments::facilitated {

FacilitatedPaymentsBottomSheetBridge::FacilitatedPaymentsBottomSheetBridge(
    content::WebContents* web_contents,
    FacilitatedPaymentsController* controller)
    : web_contents_(web_contents->GetWeakPtr()), controller_(controller) {}

FacilitatedPaymentsBottomSheetBridge::~FacilitatedPaymentsBottomSheetBridge() =
    default;

bool FacilitatedPaymentsBottomSheetBridge::RequestShowContent(
    base::span<const autofill::BankAccount> bank_account_suggestions) {
  if (java_bridge_) {
    return false;  // Already shown.
  }

  if (web_contents_ == nullptr) {
    return false;
  }
  if (!web_contents_->GetNativeView() ||
      !web_contents_->GetNativeView()->GetWindowAndroid()) {
    return false;  // No window attached (yet or anymore).
  }

  DCHECK(controller_);
  base::android::ScopedJavaLocalRef<jobject> java_controller =
      controller_->GetJavaObject();
  if (!java_controller) {
    return false;
  }

  if (web_contents_->GetBrowserContext() == nullptr) {
    return false;
  }

  Profile* browser_profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  if (browser_profile == nullptr) {
    return false;
  }

  JNIEnv* env = base::android::AttachCurrentThread();

  java_bridge_.Reset(Java_FacilitatedPaymentsPaymentMethodsViewBridge_create(
      env, java_controller,
      web_contents_->GetTopLevelNativeWindow()->GetJavaObject(),
      browser_profile->GetJavaObject()));
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

void FacilitatedPaymentsBottomSheetBridge::OnDismissed() {
  java_bridge_.Reset();
}

}  // namespace payments::facilitated
