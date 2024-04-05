// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_payment_method_view_impl.h"

#include "chrome/browser/autofill/android/personal_data_manager_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/touch_to_fill/autofill/android/internal/jni/TouchToFillPaymentMethodViewBridge_jni.h"
#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_payment_method_view_controller.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"

namespace autofill {

TouchToFillPaymentMethodViewImpl::TouchToFillPaymentMethodViewImpl(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {
  DCHECK(web_contents);
}

TouchToFillPaymentMethodViewImpl::~TouchToFillPaymentMethodViewImpl() {
  Hide();
}

bool TouchToFillPaymentMethodViewImpl::Show(
    TouchToFillPaymentMethodViewController* controller,
    base::span<const autofill::CreditCard> cards_to_suggest,
    bool should_show_scan_credit_card) {
  if (java_object_)
    return false;  // Already shown.

  if (!web_contents_->GetNativeView() ||
      !web_contents_->GetNativeView()->GetWindowAndroid()) {
    return false;  // No window attached (yet or anymore).
  }

  DCHECK(controller);
  base::android::ScopedJavaLocalRef<jobject> java_controller =
      controller->GetJavaObject();
  if (!java_controller)
    return false;

  JNIEnv* env = base::android::AttachCurrentThread();
  java_object_.Reset(Java_TouchToFillPaymentMethodViewBridge_create(
      env, java_controller,
      ProfileAndroid::FromProfile(
          Profile::FromBrowserContext(web_contents_->GetBrowserContext()))
          ->GetJavaObject(),
      web_contents_->GetTopLevelNativeWindow()->GetJavaObject()));
  if (!java_object_)
    return false;

  base::android::ScopedJavaLocalRef<jobjectArray> credit_cards_array =
      Java_TouchToFillPaymentMethodViewBridge_createCreditCardsArray(
          env, cards_to_suggest.size());
  for (size_t i = 0; i < cards_to_suggest.size(); ++i) {
    Java_TouchToFillPaymentMethodViewBridge_setCreditCard(
        env, credit_cards_array, i,
        PersonalDataManagerAndroid::CreateJavaCreditCardFromNative(
            env, cards_to_suggest[i]));
  }
  Java_TouchToFillPaymentMethodViewBridge_showSheet(
      env, java_object_, credit_cards_array, should_show_scan_credit_card);
  return true;
}

void TouchToFillPaymentMethodViewImpl::Hide() {
  if (java_object_) {
    Java_TouchToFillPaymentMethodViewBridge_hideSheet(
        base::android::AttachCurrentThread(), java_object_);
  }
}

}  // namespace autofill
