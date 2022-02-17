// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/preferences/autofill/autofill_payment_methods_delegate.h"

#include <memory>

#include "chrome/android/chrome_jni_headers/AutofillPaymentMethodsDelegate_jni.h"
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

using base::android::JavaParamRef;

namespace autofill {

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
    int64_t instrumentId) {
  CreditCard* credit_card =
      personal_data_manager_->GetCreditCardByInstrumentId(instrumentId);
  virtual_card_enrollment_manager_->OfferVirtualCardEnroll(
      *credit_card, VirtualCardEnrollmentSource::kSettingsPage,
      profile_->GetPrefs(), base::BindOnce(&risk_util::LoadRiskDataHelper));
}

void AutofillPaymentMethodsDelegate::UnenrollVirtualCard(JNIEnv* env,
                                                         int64_t instrumentId) {
  virtual_card_enrollment_manager_->Unenroll(instrumentId);
}
}  // namespace autofill
