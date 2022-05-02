// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/preferences/autofill/autofill_payment_methods_delegate.h"

#include <memory>

#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/AutofillPaymentMethodsDelegate_jni.h"
#include "chrome/android/chrome_jni_headers/VirtualCardEnrollmentFields_jni.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/autofill/risk_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_flow.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace autofill {

// static
ScopedJavaLocalRef<jobject> GetVirtualCardEnrollmentFieldsJavaObject(
    autofill::VirtualCardEnrollmentFields* virtual_card_enrollment_fields) {
  JNIEnv* env = AttachCurrentThread();
  // Create VirtualCardEnrollmentFields java object.
  ScopedJavaLocalRef<jobject> java_object =
      Java_VirtualCardEnrollmentFields_create(
          env,
          ConvertUTF16ToJavaString(
              env, virtual_card_enrollment_fields->credit_card
                       .CardIdentifierStringForAutofillDisplay()),
          gfx::ConvertToJavaBitmap(
              *virtual_card_enrollment_fields->card_art_image->bitmap()));
  // Add Google legal messages.
  for (const auto& legal_message_line :
       virtual_card_enrollment_fields->google_legal_message) {
    Java_VirtualCardEnrollmentFields_addGoogleLegalMessageLine(
        env, java_object,
        ConvertUTF16ToJavaString(env, legal_message_line.text()));
    for (const auto& link : legal_message_line.links()) {
      Java_VirtualCardEnrollmentFields_addLinkToLastGoogleLegalMessageLine(
          env, java_object, link.range.start(), link.range.end(),
          ConvertUTF8ToJavaString(env, link.url.spec()));
    }
  }
  // Add issuer legal messages.
  for (const auto& legal_message_line :
       virtual_card_enrollment_fields->issuer_legal_message) {
    Java_VirtualCardEnrollmentFields_addIssuerLegalMessageLine(
        env, java_object,
        ConvertUTF16ToJavaString(env, legal_message_line.text()));
    for (const auto& link : legal_message_line.links()) {
      Java_VirtualCardEnrollmentFields_addLinkToLastIssuerLegalMessageLine(
          env, java_object, link.range.start(), link.range.end(),
          ConvertUTF8ToJavaString(env, link.url.spec()));
    }
  }
  return java_object;
}

// static
void RunVirtualCardEnrollmentFieldsLoadedCallback(
    const JavaRef<jobject>& j_callback,
    VirtualCardEnrollmentFields* virtual_card_enrollment_fields) {
  RunObjectCallbackAndroid(j_callback, GetVirtualCardEnrollmentFieldsJavaObject(
                                           virtual_card_enrollment_fields));
}

AutofillPaymentMethodsDelegate::AutofillPaymentMethodsDelegate(Profile* profile)
    : profile_(profile) {
  personal_data_manager_ = PersonalDataManagerFactory::GetForProfile(profile);
  payments_client_ = std::make_unique<payments::PaymentsClient>(
      profile->GetURLLoaderFactory(),
      IdentityManagerFactory::GetForProfile(profile), personal_data_manager_);
  virtual_card_enrollment_manager_ =
      std::make_unique<VirtualCardEnrollmentManager>(personal_data_manager_,
                                                     payments_client_.get());
}

AutofillPaymentMethodsDelegate::~AutofillPaymentMethodsDelegate() = default;

// Initializes an instance of AutofillPaymentMethodsDelegate from the
// Java side.
static jlong JNI_AutofillPaymentMethodsDelegate_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile) {
  AutofillPaymentMethodsDelegate* instance = new AutofillPaymentMethodsDelegate(
      ProfileAndroid::FromProfileAndroid(j_profile));
  return reinterpret_cast<intptr_t>(instance);
}

void AutofillPaymentMethodsDelegate::Cleanup(JNIEnv* env) {
  delete this;
}

void AutofillPaymentMethodsDelegate::OfferVirtualCardEnrollment(
    JNIEnv* env,
    int64_t instrument_id,
    const JavaParamRef<jobject>& jcallback) {
  CreditCard* credit_card =
      personal_data_manager_->GetCreditCardByInstrumentId(instrument_id);
  virtual_card_enrollment_manager_->OfferVirtualCardEnroll(
      *credit_card, VirtualCardEnrollmentSource::kSettingsPage,
      profile_->GetPrefs(), base::BindOnce(&risk_util::LoadRiskDataHelper),
      base::BindOnce(&RunVirtualCardEnrollmentFieldsLoadedCallback,
                     ScopedJavaGlobalRef<jobject>(jcallback)));
}

void AutofillPaymentMethodsDelegate::EnrollOfferedVirtualCard(JNIEnv* env) {
  virtual_card_enrollment_manager_->Enroll();
}

void AutofillPaymentMethodsDelegate::UnenrollVirtualCard(
    JNIEnv* env,
    int64_t instrument_id) {
  virtual_card_enrollment_manager_->Unenroll(instrument_id);
}
}  // namespace autofill
