// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/payments/android/touch_to_fill_credit_card_view_impl.h"

#include "chrome/browser/autofill/android/personal_data_manager_android.h"
#include "chrome/browser/touch_to_fill/payments/android/jni_headers/TouchToFillCreditCardViewBridge_jni.h"
#include "chrome/browser/touch_to_fill/payments/android/touch_to_fill_credit_card_view_controller.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"

namespace autofill {

TouchToFillCreditCardViewImpl::TouchToFillCreditCardViewImpl(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {
  DCHECK(web_contents);
  DCHECK(base::FeatureList::IsEnabled(
      features::kAutofillTouchToFillForCreditCardsAndroid));
}

TouchToFillCreditCardViewImpl::~TouchToFillCreditCardViewImpl() {
  Hide();
}

bool TouchToFillCreditCardViewImpl::Show(
    TouchToFillCreditCardViewController* controller,
    base::span<const autofill::CreditCard* const> cards_to_suggest,
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
  java_object_.Reset(Java_TouchToFillCreditCardViewBridge_create(
      env, java_controller,
      web_contents_->GetTopLevelNativeWindow()->GetJavaObject()));
  if (!java_object_)
    return false;

  base::android::ScopedJavaLocalRef<jobjectArray> credit_cards_array =
      Java_TouchToFillCreditCardViewBridge_createCreditCardsArray(
          env, cards_to_suggest.size());
  for (size_t i = 0; i < cards_to_suggest.size(); ++i) {
    Java_TouchToFillCreditCardViewBridge_setCreditCard(
        env, credit_cards_array, i,
        PersonalDataManagerAndroid::CreateJavaCreditCardFromNative(
            env, *cards_to_suggest[i]));
  }
  Java_TouchToFillCreditCardViewBridge_showSheet(
      env, java_object_, credit_cards_array, should_show_scan_credit_card);
  return true;
}

void TouchToFillCreditCardViewImpl::Hide() {
  if (java_object_) {
    Java_TouchToFillCreditCardViewBridge_hideSheet(
        base::android::AttachCurrentThread(), java_object_);
  }
}

}  // namespace autofill
