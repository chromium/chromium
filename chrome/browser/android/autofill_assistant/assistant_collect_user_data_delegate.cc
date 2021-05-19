// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/autofill_assistant/assistant_collect_user_data_delegate.h"

#include <memory>
#include <utility>

#include "base/android/jni_string.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/android/features/autofill_assistant/jni_headers/AssistantCollectUserDataNativeDelegate_jni.h"
#include "chrome/browser/android/autofill_assistant/ui_controller_android.h"
#include "chrome/browser/android/autofill_assistant/ui_controller_android_utils.h"
#include "chrome/browser/autofill/android/personal_data_manager_android.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/autofill/core/browser/autofill_data_util.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;

namespace autofill_assistant {

AssistantCollectUserDataDelegate::AssistantCollectUserDataDelegate(
    UiControllerAndroid* ui_controller)
    : ui_controller_(ui_controller) {
  java_assistant_collect_user_data_delegate_ =
      Java_AssistantCollectUserDataNativeDelegate_create(
          AttachCurrentThread(), reinterpret_cast<intptr_t>(this));
}

AssistantCollectUserDataDelegate::~AssistantCollectUserDataDelegate() {
  Java_AssistantCollectUserDataNativeDelegate_clearNativePtr(
      AttachCurrentThread(), java_assistant_collect_user_data_delegate_);
}

void AssistantCollectUserDataDelegate::OnContactInfoChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jobject>& jcontact_profile) {
  if (!jcontact_profile) {
    ui_controller_->OnContactInfoChanged(nullptr);
    return;
  }

  auto contact_profile = std::make_unique<autofill::AutofillProfile>();
  autofill::PersonalDataManagerAndroid::PopulateNativeProfileFromJava(
      jcontact_profile, env, contact_profile.get());

  ui_controller_->OnContactInfoChanged(std::move(contact_profile));
}

void AssistantCollectUserDataDelegate::OnShippingAddressChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jobject>& jaddress) {
  if (!jaddress) {
    ui_controller_->OnShippingAddressChanged(nullptr);
    return;
  }

  auto shipping_address = std::make_unique<autofill::AutofillProfile>();
  autofill::PersonalDataManagerAndroid::PopulateNativeProfileFromJava(
      jaddress, env, shipping_address.get());
  ui_controller_->OnShippingAddressChanged(std::move(shipping_address));
}

void AssistantCollectUserDataDelegate::OnCreditCardChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jobject>& jcard,
    const base::android::JavaParamRef<jobject>& jbilling_profile) {
  std::unique_ptr<autofill::CreditCard> card;
  if (jcard) {
    card = std::make_unique<autofill::CreditCard>();
    autofill::PersonalDataManagerAndroid::PopulateNativeCreditCardFromJava(
        jcard, env, card.get());
  }

  std::unique_ptr<autofill::AutofillProfile> billing_profile;
  if (jbilling_profile) {
    billing_profile = std::make_unique<autofill::AutofillProfile>();
    autofill::PersonalDataManagerAndroid::PopulateNativeProfileFromJava(
        jbilling_profile, env, billing_profile.get());
  }

  ui_controller_->OnCreditCardChanged(std::move(card),
                                      std::move(billing_profile));
}

void AssistantCollectUserDataDelegate::OnTermsAndConditionsChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jint state) {
  ui_controller_->OnTermsAndConditionsChanged(
      static_cast<TermsAndConditionsState>(state));
}

void AssistantCollectUserDataDelegate::OnTextLinkClicked(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jint link) {
  ui_controller_->OnTextLinkClicked(link);
}

void AssistantCollectUserDataDelegate::OnLoginChoiceChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jstring>& jidentifier) {
  std::string identifier =
      ui_controller_android_utils::SafeConvertJavaStringToNative(env,
                                                                 jidentifier);
  ui_controller_->OnLoginChoiceChanged(identifier);
}

void AssistantCollectUserDataDelegate::OnDateTimeRangeStartDateChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jint year,
    jint month,
    jint day) {
  ui_controller_->OnDateTimeRangeStartDateChanged(year, month, day);
}

void AssistantCollectUserDataDelegate::OnDateTimeRangeStartDateCleared(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  ui_controller_->OnDateTimeRangeStartDateCleared();
}

void AssistantCollectUserDataDelegate::OnDateTimeRangeStartTimeSlotChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jint index) {
  ui_controller_->OnDateTimeRangeStartTimeSlotChanged(index);
}

void AssistantCollectUserDataDelegate::OnDateTimeRangeStartTimeSlotCleared(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  ui_controller_->OnDateTimeRangeStartTimeSlotCleared();
}

void AssistantCollectUserDataDelegate::OnDateTimeRangeEndDateChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jint year,
    jint month,
    jint day) {
  ui_controller_->OnDateTimeRangeEndDateChanged(year, month, day);
}

void AssistantCollectUserDataDelegate::OnDateTimeRangeEndDateCleared(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  ui_controller_->OnDateTimeRangeEndDateCleared();
}

void AssistantCollectUserDataDelegate::OnDateTimeRangeEndTimeSlotChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jint index) {
  ui_controller_->OnDateTimeRangeEndTimeSlotChanged(index);
}

void AssistantCollectUserDataDelegate::OnDateTimeRangeEndTimeSlotCleared(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  ui_controller_->OnDateTimeRangeEndTimeSlotCleared();
}

void AssistantCollectUserDataDelegate::OnKeyValueChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jstring>& jkey,
    const base::android::JavaParamRef<jobject>& jvalue) {
  ui_controller_->OnKeyValueChanged(
      ui_controller_android_utils::SafeConvertJavaStringToNative(env, jkey),
      ui_controller_android_utils::ToNativeValue(env, jvalue));
}

void AssistantCollectUserDataDelegate::OnInputTextFocusChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jboolean jis_focused) {
  ui_controller_->OnInputTextFocusChanged(jis_focused);
}

base::android::ScopedJavaGlobalRef<jobject>
AssistantCollectUserDataDelegate::GetJavaObject() {
  return java_assistant_collect_user_data_delegate_;
}

bool AssistantCollectUserDataDelegate::IsContactComplete(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jobject>& jcontact_profile) {
  if (!jcontact_profile) {
    return ui_controller_->IsContactComplete(nullptr);
  }

  autofill::AutofillProfile contact;
  autofill::PersonalDataManagerAndroid::PopulateNativeProfileFromJava(
      jcontact_profile, env, &contact);

  return ui_controller_->IsContactComplete(&contact);
}

bool AssistantCollectUserDataDelegate::IsShippingAddressComplete(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jobject>& jaddress) {
  if (!jaddress) {
    return ui_controller_->IsShippingAddressComplete(nullptr);
  }

  autofill::AutofillProfile address;
  autofill::PersonalDataManagerAndroid::PopulateNativeProfileFromJava(
      jaddress, env, &address);

  return ui_controller_->IsShippingAddressComplete(&address);
}

bool AssistantCollectUserDataDelegate::IsPaymentInstrumentComplete(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jobject>& jcard,
    const base::android::JavaParamRef<jobject>& jaddress) {
  if (!jcard) {
    return ui_controller_->IsPaymentInstrumentComplete(nullptr, nullptr);
  }

  autofill::CreditCard card;
  autofill::PersonalDataManagerAndroid::PopulateNativeCreditCardFromJava(
      jcard, env, &card);

  if (jaddress) {
    autofill::AutofillProfile address;
    autofill::PersonalDataManagerAndroid::PopulateNativeProfileFromJava(
        jaddress, env, &address);

    return ui_controller_->IsPaymentInstrumentComplete(&card, &address);
  }

  return ui_controller_->IsPaymentInstrumentComplete(&card, nullptr);
}

}  // namespace autofill_assistant
