// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/preferences/autofill/autofill_payment_methods_delegate.h"

#include <memory>
#include <optional>

#include "base/android/callback_android.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/android/autofill/virtual_card_utils.h"
#include "chrome/browser/ui/autofill/risk_util.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_flow.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/AutofillPaymentMethodsDelegate_jni.h"

using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;

namespace autofill {
namespace {
void RunVirtualCardEnrollmentUpdateResponseCallback(
    ScopedJavaGlobalRef<jobject> callback,
    payments::PaymentsAutofillClient::PaymentsRpcResult result) {
  base::android::RunBooleanCallbackAndroid(
      callback,
      result == payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess);
}

void RunVirtualCardEnrollmentFieldsLoadedCallback(
    const JavaRef<jobject>& j_callback,
    VirtualCardEnrollmentFields* virtual_card_enrollment_fields) {
  base::android::RunObjectCallbackAndroid(
      j_callback, autofill::CreateVirtualCardEnrollmentFieldsJavaObject(
                      virtual_card_enrollment_fields));
}
}  // namespace

AutofillPaymentMethodsDelegate::AutofillPaymentMethodsDelegate(Profile* profile)
    : profile_(profile) {
  personal_data_manager_ =
      PersonalDataManagerFactory::GetForBrowserContext(profile);
  payments_network_interface_ =
      std::make_unique<payments::PaymentsNetworkInterface>(
          profile->GetURLLoaderFactory(),
          IdentityManagerFactory::GetForProfile(profile),
          &personal_data_manager_->payments_data_manager());
  virtual_card_enrollment_manager_ =
      std::make_unique<VirtualCardEnrollmentManager>(
          personal_data_manager_, payments_network_interface_.get());
}

AutofillPaymentMethodsDelegate::~AutofillPaymentMethodsDelegate() = default;

// Initializes an instance of AutofillPaymentMethodsDelegate from the
// Java side.
static jlong JNI_AutofillPaymentMethodsDelegate_Init(JNIEnv* env,
                                                     Profile* profile) {
  AutofillPaymentMethodsDelegate* instance =
      new AutofillPaymentMethodsDelegate(profile);
  return reinterpret_cast<intptr_t>(instance);
}

void AutofillPaymentMethodsDelegate::Cleanup(JNIEnv* env) {
  delete this;
}

void AutofillPaymentMethodsDelegate::InitVirtualCardEnrollment(
    JNIEnv* env,
    int64_t instrument_id,
    const JavaParamRef<jobject>& jcallback) {
  const CreditCard* credit_card =
      personal_data_manager_->payments_data_manager()
          .GetCreditCardByInstrumentId(instrument_id);
  virtual_card_enrollment_manager_->InitVirtualCardEnroll(
      *credit_card, VirtualCardEnrollmentSource::kSettingsPage, std::nullopt,
      profile_->GetPrefs(), base::BindOnce(&risk_util::LoadRiskDataHelper),
      base::BindOnce(&RunVirtualCardEnrollmentFieldsLoadedCallback,
                     ScopedJavaGlobalRef<jobject>(jcallback)));
}

void AutofillPaymentMethodsDelegate::EnrollOfferedVirtualCard(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcallback) {
  virtual_card_enrollment_manager_->Enroll(
      base::BindOnce(&RunVirtualCardEnrollmentUpdateResponseCallback,
                     ScopedJavaGlobalRef<jobject>(jcallback)));
}

void AutofillPaymentMethodsDelegate::UnenrollVirtualCard(
    JNIEnv* env,
    int64_t instrument_id,
    const JavaParamRef<jobject>& jcallback) {
  virtual_card_enrollment_manager_->Unenroll(
      instrument_id,
      base::BindOnce(&RunVirtualCardEnrollmentUpdateResponseCallback,
                     ScopedJavaGlobalRef<jobject>(jcallback)));
}

void AutofillPaymentMethodsDelegate::DeleteSavedCvcs(JNIEnv* env) {
  personal_data_manager_->payments_data_manager().ClearLocalCvcs();
  personal_data_manager_->payments_data_manager().ClearServerCvcs();
}
}  // namespace autofill
