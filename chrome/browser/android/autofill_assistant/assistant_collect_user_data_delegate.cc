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
#include "chrome/browser/autofill/android/personal_data_manager_android.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/autofill/core/browser/autofill_data_util.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;

namespace {
// Converts a java string to native. Returns an empty string if input is null.
std::string SafeConvertJavaStringToNative(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& jstring) {
  std::string native_string;
  if (jstring) {
    base::android::ConvertJavaStringToUTF8(env, jstring, &native_string);
  }
  return native_string;
}
}  // namespace

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
    const base::android::JavaParamRef<jstring>& jpayer_name,
    const base::android::JavaParamRef<jstring>& jpayer_phone,
    const base::android::JavaParamRef<jstring>& jpayer_email) {
  std::string name = SafeConvertJavaStringToNative(env, jpayer_name);
  std::string phone = SafeConvertJavaStringToNative(env, jpayer_phone);
  std::string email = SafeConvertJavaStringToNative(env, jpayer_email);

  auto contact_profile = std::make_unique<autofill::AutofillProfile>();
  contact_profile->SetRawInfo(autofill::ServerFieldType::NAME_FULL,
                              base::UTF8ToUTF16(name));
  autofill::data_util::NameParts parts =
      autofill::data_util::SplitName(base::UTF8ToUTF16(name));
  contact_profile->SetRawInfo(autofill::ServerFieldType::NAME_FIRST,
                              parts.given);
  contact_profile->SetRawInfo(autofill::ServerFieldType::NAME_MIDDLE,
                              parts.middle);
  contact_profile->SetRawInfo(autofill::ServerFieldType::NAME_LAST,
                              parts.family);
  contact_profile->SetRawInfo(autofill::ServerFieldType::EMAIL_ADDRESS,
                              base::UTF8ToUTF16(email));
  contact_profile->SetRawInfo(
      autofill::ServerFieldType::PHONE_HOME_WHOLE_NUMBER,
      base::UTF8ToUTF16(phone));
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
  std::unique_ptr<autofill::CreditCard> card = nullptr;
  if (jcard) {
    card = std::make_unique<autofill::CreditCard>();
    autofill::PersonalDataManagerAndroid::PopulateNativeCreditCardFromJava(
        jcard, env, card.get());
  }

  std::unique_ptr<autofill::AutofillProfile> billing_profile = nullptr;
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

void AssistantCollectUserDataDelegate::OnTermsAndConditionsLinkClicked(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jint link) {
  ui_controller_->OnTermsAndConditionsLinkClicked(link);
}

void AssistantCollectUserDataDelegate::OnLoginChoiceChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jstring>& jidentifier) {
  std::string identifier = SafeConvertJavaStringToNative(env, jidentifier);
  ui_controller_->OnLoginChoiceChanged(identifier);
}

void AssistantCollectUserDataDelegate::OnDateTimeRangeStartChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jint year,
    jint month,
    jint day,
    jint hour,
    jint minute,
    jint second) {
  ui_controller_->OnDateTimeRangeStartChanged(year, month, day, hour, minute,
                                              second);
}

void AssistantCollectUserDataDelegate::OnDateTimeRangeEndChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jint year,
    jint month,
    jint day,
    jint hour,
    jint minute,
    jint second) {
  ui_controller_->OnDateTimeRangeEndChanged(year, month, day, hour, minute,
                                            second);
}

void AssistantCollectUserDataDelegate::OnKeyValueChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jstring>& jkey,
    const base::android::JavaParamRef<jstring>& jvalue) {
  ui_controller_->OnKeyValueChanged(SafeConvertJavaStringToNative(env, jkey),
                                    SafeConvertJavaStringToNative(env, jvalue));
}

base::android::ScopedJavaGlobalRef<jobject>
AssistantCollectUserDataDelegate::GetJavaObject() {
  return java_assistant_collect_user_data_delegate_;
}

}  // namespace autofill_assistant
