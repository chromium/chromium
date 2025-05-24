// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include <memory>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/autofill/android/personal_data_manager_android.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/autofill/test/jni_headers/AutofillTestHelper_jni.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/bank_account.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/data_model/payments/ewallet.h"
#include "components/autofill/core/common/autofill_clock.h"

namespace autofill {
namespace {

using ::base::android::ConvertJavaStringToUTF16;
using ::base::android::ConvertJavaStringToUTF8;
using ::base::android::JavaParamRef;

PersonalDataManager* GetPersonalDataManagerForLastUsedProfile() {
  return PersonalDataManagerFactory::GetForBrowserContext(
      ProfileManager::GetLastUsedProfile());
}

}  // anonymous namespace

// static
jlong JNI_AutofillTestHelper_GetDateNDaysAgo(JNIEnv* env, jint days) {
  return (AutofillClock::Now() - base::Days(days)).ToTimeT();
}

// static
void JNI_AutofillTestHelper_AddServerCreditCard(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcard) {
  std::unique_ptr<CreditCard> card = std::make_unique<CreditCard>();
  PersonalDataManagerAndroid::PopulateNativeCreditCardFromJava(jcard, env,
                                                               card.get());
  card->set_record_type(CreditCard::RecordType::kMaskedServerCard);
  PersonalDataManager* personal_data_manager =
      GetPersonalDataManagerForLastUsedProfile();
  personal_data_manager->payments_data_manager().AddServerCreditCardForTest(
      std::move(card));
  personal_data_manager->NotifyPersonalDataObserver();
}

// static
void JNI_AutofillTestHelper_AddServerCreditCardWithAdditionalFields(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcard,
    std::u16string& nickname,
    jint jcard_issuer) {
  std::unique_ptr<CreditCard> card = std::make_unique<CreditCard>();
  PersonalDataManagerAndroid::PopulateNativeCreditCardFromJava(jcard, env,
                                                               card.get());
  card->set_record_type(CreditCard::RecordType::kMaskedServerCard);
  card->SetNickname(nickname);
  card->set_card_issuer(static_cast<CreditCard::Issuer>(jcard_issuer));
  PersonalDataManager* personal_data_manager =
      GetPersonalDataManagerForLastUsedProfile();
  personal_data_manager->payments_data_manager().AddServerCreditCardForTest(
      std::move(card));
  personal_data_manager->NotifyPersonalDataObserver();
}

// static
void JNI_AutofillTestHelper_SetProfileUseStats(JNIEnv* env,
                                               std::string& guid,
                                               jint count,
                                               jint days_since_last_used) {
  DCHECK(count >= 0 && days_since_last_used >= 0);

  PersonalDataManager* personal_data_manager =
      GetPersonalDataManagerForLastUsedProfile();
  AutofillProfile profile =
      *personal_data_manager->address_data_manager().GetProfileByGUID(guid);
  profile.usage_history().set_use_count(static_cast<size_t>(count));
  profile.usage_history().set_use_date(AutofillClock::Now() -
                                       base::Days(days_since_last_used));
  personal_data_manager->address_data_manager().UpdateProfile(profile);
}

// static
jint JNI_AutofillTestHelper_GetProfileUseCount(JNIEnv* env, std::string& guid) {
  PersonalDataManager* personal_data_manager =
      GetPersonalDataManagerForLastUsedProfile();
  const AutofillProfile* profile =
      personal_data_manager->address_data_manager().GetProfileByGUID(guid);
  return profile->usage_history().use_count();
}

// static
jlong JNI_AutofillTestHelper_GetProfileUseDate(JNIEnv* env, std::string& guid) {
  PersonalDataManager* personal_data_manager =
      GetPersonalDataManagerForLastUsedProfile();
  const AutofillProfile* profile =
      personal_data_manager->address_data_manager().GetProfileByGUID(guid);
  return profile->usage_history().use_date().ToTimeT();
}

// static
std::string JNI_AutofillTestHelper_AddCreditCardWithUseStats(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcard,
    jint count,
    jint days_since_last_used) {
  DCHECK(count >= 0 && days_since_last_used >= 0);

  CreditCard card;
  PersonalDataManagerAndroid::PopulateNativeCreditCardFromJava(jcard, env, &card);

  card.usage_history().set_use_count(static_cast<size_t>(count));
  card.usage_history().set_use_date(AutofillClock::Now() -
                                    base::Days(days_since_last_used));

  PersonalDataManager* personal_data_manager =
      GetPersonalDataManagerForLastUsedProfile();
  personal_data_manager->payments_data_manager().AddCreditCard(card);
  personal_data_manager->NotifyPersonalDataObserver();

  return card.guid();
}

// static
jint JNI_AutofillTestHelper_GetCreditCardUseCount(JNIEnv* env,
                                                  std::string& guid) {
  PersonalDataManager* personal_data_manager =
      GetPersonalDataManagerForLastUsedProfile();
  const CreditCard* card =
      personal_data_manager->payments_data_manager().GetCreditCardByGUID(guid);
  return card->usage_history().use_count();
}

// static
jlong JNI_AutofillTestHelper_GetCreditCardUseDate(JNIEnv* env,
                                                  std::string& guid) {
  PersonalDataManager* personal_data_manager =
      GetPersonalDataManagerForLastUsedProfile();
  const CreditCard* card =
      personal_data_manager->payments_data_manager().GetCreditCardByGUID(guid);
  return card->usage_history().use_date().ToTimeT();
}

// TODO(crbug.com/40477114): Use a mock clock for testing.
jlong JNI_AutofillTestHelper_GetCurrentDate(JNIEnv* env) {
  return base::Time::Now().ToTimeT();
}

// static
void JNI_AutofillTestHelper_ClearServerData(JNIEnv* env) {
  PersonalDataManager* personal_data_manager =
      GetPersonalDataManagerForLastUsedProfile();
  personal_data_manager->payments_data_manager().ClearAllServerDataForTesting();
  personal_data_manager->NotifyPersonalDataObserver();
}

// static
void JNI_AutofillTestHelper_SetSyncService(JNIEnv* env) {
  GetPersonalDataManagerForLastUsedProfile()
      ->payments_data_manager()
      .SetSyncingForTest(true);
}

// static
void JNI_AutofillTestHelper_AddMaskedBankAccount(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbank_account) {
  BankAccount bank_account =
      PersonalDataManagerAndroid::CreateNativeBankAccountFromJava(
          env, jbank_account);
  PersonalDataManager* personal_data_manager =
      GetPersonalDataManagerForLastUsedProfile();
  personal_data_manager->payments_data_manager().AddMaskedBankAccountForTest(
      bank_account);
  personal_data_manager->NotifyPersonalDataObserver();
}

// static
void JNI_AutofillTestHelper_AddEwallet(JNIEnv* env,
                                       const JavaParamRef<jobject>& jewallet) {
  Ewallet ewallet =
      PersonalDataManagerAndroid::CreateNativeEwalletFromJava(env, jewallet);
  PersonalDataManager* personal_data_manager =
      GetPersonalDataManagerForLastUsedProfile();
  personal_data_manager->payments_data_manager().AddEwalletForTest(ewallet);
  personal_data_manager->NotifyPersonalDataObserver();
}

}  // namespace autofill
