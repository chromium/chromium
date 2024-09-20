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

bool FacilitatedPaymentsBottomSheetBridge::IsInLandscapeMode() {
  if (!GetJavaBridge()) {
    return false;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_FacilitatedPaymentsPaymentMethodsViewBridge_isInLandscapeMode(
      env, GetJavaBridge());
}

bool FacilitatedPaymentsBottomSheetBridge::RequestShowContent(
    base::span<const autofill::BankAccount> bank_account_suggestions) {
  if (!GetJavaBridge()) {
    return false;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  std::vector<base::android::ScopedJavaLocalRef<jobject>> bank_accounts_array;
  bank_accounts_array.reserve(bank_account_suggestions.size());
  for (const autofill::BankAccount& bank_account : bank_account_suggestions) {
    bank_accounts_array.push_back(
        autofill::PersonalDataManagerAndroid::CreateJavaBankAccountFromNative(
            env, bank_account));
  }
  return Java_FacilitatedPaymentsPaymentMethodsViewBridge_requestShowContent(
      env, GetJavaBridge(), std::move(bank_accounts_array));
}

void FacilitatedPaymentsBottomSheetBridge::ShowProgressScreen() {
  if (!GetJavaBridge()) {
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_FacilitatedPaymentsPaymentMethodsViewBridge_showProgressScreen(
      env, GetJavaBridge());
}

void FacilitatedPaymentsBottomSheetBridge::ShowErrorScreen() {
  if (!GetJavaBridge()) {
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_FacilitatedPaymentsPaymentMethodsViewBridge_showErrorScreen(
      env, GetJavaBridge());
}

void FacilitatedPaymentsBottomSheetBridge::Dismiss() {
  // If the Java bridge doesn't exist, the bottom sheet isn't open.
  if (!java_bridge_) {
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_FacilitatedPaymentsPaymentMethodsViewBridge_dismiss(env, java_bridge_);
}

base::android::ScopedJavaLocalRef<jobject>
FacilitatedPaymentsBottomSheetBridge::GetJavaBridge() {
  if (!java_bridge_) {
    if (!web_contents_ || !web_contents_->GetNativeView() ||
        !web_contents_->GetNativeView()->GetWindowAndroid() ||
        !web_contents_->GetBrowserContext()) {
      return nullptr;
    }

    Profile* browser_profile =
        Profile::FromBrowserContext(web_contents_->GetBrowserContext());
    if (!browser_profile) {
      return nullptr;
    }

    base::android::ScopedJavaLocalRef<jobject> java_controller =
        controller_->GetJavaObject();
    if (!java_controller) {
      return nullptr;
    }

    JNIEnv* env = base::android::AttachCurrentThread();
    java_bridge_.Reset(Java_FacilitatedPaymentsPaymentMethodsViewBridge_create(
        env, java_controller,
        web_contents_->GetTopLevelNativeWindow()->GetJavaObject(),
        browser_profile->GetJavaObject()));
  }
  return base::android::ScopedJavaLocalRef<jobject>(java_bridge_);
}

void FacilitatedPaymentsBottomSheetBridge::OnDismissed() {
  java_bridge_.Reset();
}

}  // namespace payments::facilitated
