// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/personal_data_manager_android.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/command_line.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/autofill/android/autofill_image_fetcher_impl.h"
#include "chrome/browser/autofill/android/jni_headers/PersonalDataManager_jni.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/address_i18n.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/ui/autofill_resource_utils.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/prefs/pref_service.h"
#include "url/android/gurl_android.h"

namespace autofill {
namespace {

using ::base::android::ConvertJavaStringToUTF8;
using ::base::android::ConvertUTF16ToJavaString;
using ::base::android::ConvertUTF8ToJavaString;
using ::base::android::JavaParamRef;
using ::base::android::JavaRef;
using ::base::android::ScopedJavaGlobalRef;
using ::base::android::ScopedJavaLocalRef;

Profile* GetProfile() {
  return ProfileManager::GetActiveUserProfile()->GetOriginalProfile();
}

PrefService* GetPrefs() {
  return GetProfile()->GetPrefs();
}

}  // namespace

PersonalDataManagerAndroid::PersonalDataManagerAndroid(JNIEnv* env, jobject obj)
    : weak_java_obj_(env, obj),
      personal_data_manager_(PersonalDataManagerFactory::GetForProfile(
          ProfileManager::GetActiveUserProfile())) {
  personal_data_manager_->AddObserver(this);
}

PersonalDataManagerAndroid::~PersonalDataManagerAndroid() {
  personal_data_manager_->RemoveObserver(this);
}

// static
ScopedJavaLocalRef<jobject>
PersonalDataManagerAndroid::CreateJavaCreditCardFromNative(
    JNIEnv* env,
    const CreditCard& card) {
  const data_util::PaymentRequestData& payment_request_data =
      data_util::GetPaymentRequestData(card.network());
  return Java_CreditCard_create(
      env, ConvertUTF8ToJavaString(env, card.guid()),
      ConvertUTF8ToJavaString(env, card.origin()),
      card.record_type() == CreditCard::RecordType::kLocalCard,
      card.record_type() == CreditCard::RecordType::kFullServerCard,
      card.record_type() == CreditCard::RecordType::kVirtualCard,
      ConvertUTF16ToJavaString(env, card.GetRawInfo(CREDIT_CARD_NAME_FULL)),
      ConvertUTF16ToJavaString(env, card.GetRawInfo(CREDIT_CARD_NUMBER)),
      ConvertUTF16ToJavaString(env, card.NetworkAndLastFourDigits()),
      ConvertUTF16ToJavaString(env, card.GetRawInfo(CREDIT_CARD_EXP_MONTH)),
      ConvertUTF16ToJavaString(env,
                               card.GetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR)),
      ConvertUTF8ToJavaString(env,
                              payment_request_data.basic_card_issuer_network),
      ResourceMapper::MapToJavaDrawableId(
          GetIconResourceID(card.CardIconForAutofillSuggestion())),
      ConvertUTF8ToJavaString(env, card.billing_address_id()),
      ConvertUTF8ToJavaString(env, card.server_id()), card.instrument_id(),
      ConvertUTF16ToJavaString(env, card.CardNameAndLastFourDigits()),
      ConvertUTF16ToJavaString(env, card.nickname()),
      url::GURLAndroid::FromNativeGURL(env, card.card_art_url()),
      static_cast<jint>(card.virtual_card_enrollment_state()),
      ConvertUTF16ToJavaString(env, card.product_description()),
      ConvertUTF16ToJavaString(env, card.CardNameForAutofillDisplay()),
      ConvertUTF16ToJavaString(
          env, card.ObfuscatedNumberWithVisibleLastFourDigits()),
      ConvertUTF16ToJavaString(env, card.cvc()));
}

// static
void PersonalDataManagerAndroid::PopulateNativeCreditCardFromJava(
    const JavaRef<jobject>& jcard,
    JNIEnv* env,
    CreditCard* card) {
  card->set_origin(
      ConvertJavaStringToUTF8(Java_CreditCard_getOrigin(env, jcard)));
  card->SetRawInfo(
      CREDIT_CARD_NAME_FULL,
      ConvertJavaStringToUTF16(Java_CreditCard_getName(env, jcard)));
  card->SetRawInfo(
      CREDIT_CARD_NUMBER,
      ConvertJavaStringToUTF16(Java_CreditCard_getNumber(env, jcard)));
  card->SetRawInfo(
      CREDIT_CARD_EXP_MONTH,
      ConvertJavaStringToUTF16(Java_CreditCard_getMonth(env, jcard)));
  card->SetRawInfo(
      CREDIT_CARD_EXP_4_DIGIT_YEAR,
      ConvertJavaStringToUTF16(Java_CreditCard_getYear(env, jcard)));
  card->set_billing_address_id(
      ConvertJavaStringToUTF8(Java_CreditCard_getBillingAddressId(env, jcard)));
  card->set_server_id(
      ConvertJavaStringToUTF8(Java_CreditCard_getServerId(env, jcard)));
  card->set_instrument_id(Java_CreditCard_getInstrumentId(env, jcard));
  card->SetNickname(
      ConvertJavaStringToUTF16(Java_CreditCard_getNickname(env, jcard)));
  base::android::ScopedJavaLocalRef<jobject> java_card_art_url =
      Java_CreditCard_getCardArtUrl(env, jcard);
  if (!java_card_art_url.is_null()) {
    card->set_card_art_url(
        *url::GURLAndroid::ToNativeGURL(env, java_card_art_url));
  }
  // Only set the guid if it is an existing card (java guid not empty).
  // Otherwise, keep the generated one.
  std::string guid =
      ConvertJavaStringToUTF8(Java_CreditCard_getGUID(env, jcard));
  if (!guid.empty())
    card->set_guid(guid);

  if (Java_CreditCard_getIsLocal(env, jcard)) {
    card->set_record_type(CreditCard::RecordType::kLocalCard);
  } else {
    if (Java_CreditCard_getIsCached(env, jcard)) {
      card->set_record_type(CreditCard::RecordType::kFullServerCard);
    } else {
      // Native copies of virtual credit card objects should not be created.
      DCHECK(!Java_CreditCard_getIsVirtual(env, jcard));
      card->set_record_type(CreditCard::RecordType::kMaskedServerCard);
      card->SetNetworkForMaskedCard(
          data_util::GetIssuerNetworkForBasicCardIssuerNetwork(
              ConvertJavaStringToUTF8(
                  env, Java_CreditCard_getBasicCardIssuerNetwork(env, jcard))));
    }
  }
  card->set_virtual_card_enrollment_state(
      static_cast<CreditCard::VirtualCardEnrollmentState>(
          Java_CreditCard_getVirtualCardEnrollmentState(env, jcard)));
  card->set_product_description(ConvertJavaStringToUTF16(
      Java_CreditCard_getProductDescription(env, jcard)));
  card->set_cvc(ConvertJavaStringToUTF16(
        Java_CreditCard_getCvc(env, jcard)));
}

jboolean PersonalDataManagerAndroid::IsDataLoaded(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& unused_obj) const {
  return personal_data_manager_->IsDataLoaded();
}

ScopedJavaLocalRef<jobjectArray>
PersonalDataManagerAndroid::GetProfileGUIDsForSettings(
    JNIEnv* env,
    const JavaParamRef<jobject>& unused_obj) {
  return GetProfileGUIDs(env, personal_data_manager_->GetProfilesForSettings());
}

ScopedJavaLocalRef<jobjectArray>
PersonalDataManagerAndroid::GetProfileGUIDsToSuggest(
    JNIEnv* env,
    const JavaParamRef<jobject>& unused_obj) {
  return GetProfileGUIDs(env, personal_data_manager_->GetProfilesToSuggest());
}

ScopedJavaLocalRef<jobject> PersonalDataManagerAndroid::GetProfileByGUID(
    JNIEnv* env,
    const JavaParamRef<jobject>& unused_obj,
    const JavaParamRef<jstring>& jguid) {
  AutofillProfile* profile = personal_data_manager_->GetProfileByGUID(
      ConvertJavaStringToUTF8(env, jguid));
  if (!profile)
    return ScopedJavaLocalRef<jobject>();

  return profile->CreateJavaObject(g_browser_process->GetApplicationLocale());
}

jboolean PersonalDataManagerAndroid::IsEligibleForAddressAccountStorage(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& unused_obj) {
  return personal_data_manager_->IsEligibleForAddressAccountStorage();
}

base::android::ScopedJavaLocalRef<jstring>
PersonalDataManagerAndroid::GetDefaultCountryCodeForNewAddress(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& unused_obj) const {
  return ConvertUTF8ToJavaString(
      env, personal_data_manager_->GetDefaultCountryCodeForNewAddress());
}

bool PersonalDataManagerAndroid::IsCountryEligibleForAccountStorage(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& unused_obj,
    const JavaParamRef<jstring>& country_code) const {
  return personal_data_manager_->IsCountryEligibleForAccountStorage(
      ConvertJavaStringToUTF8(env, country_code));
}

ScopedJavaLocalRef<jstring> PersonalDataManagerAndroid::SetProfile(
    JNIEnv* env,
    const JavaParamRef<jobject>& unused_obj,
    const JavaParamRef<jobject>& jprofile,
    const JavaParamRef<jstring>& jguid) {
  std::string guid = ConvertJavaStringToUTF8(env, jguid);

  AutofillProfile profile = AutofillProfile::CreateFromJavaObject(
      jprofile, personal_data_manager_->GetProfileByGUID(guid),
      g_browser_process->GetApplicationLocale());

  if (guid.empty()) {
    personal_data_manager_->AddProfile(profile);
  } else {
    personal_data_manager_->UpdateProfile(profile);
  }

  return ConvertUTF8ToJavaString(env, profile.guid());
}

ScopedJavaLocalRef<jstring> PersonalDataManagerAndroid::SetProfileToLocal(
    JNIEnv* env,
    const JavaParamRef<jobject>& unused_obj,
    const JavaParamRef<jobject>& jprofile,
    const JavaParamRef<jstring>& jguid) {
  const AutofillProfile* target_profile =
      personal_data_manager_->GetProfileByGUID(
          ConvertJavaStringToUTF8(env, jguid));
  AutofillProfile profile = AutofillProfile::CreateFromJavaObject(
      jprofile, target_profile, g_browser_process->GetApplicationLocale());

  if (target_profile != nullptr) {
    personal_data_manager_->UpdateProfile(profile);
  } else {
    personal_data_manager_->AddProfile(profile);
  }

  return ConvertUTF8ToJavaString(env, profile.guid());
}

ScopedJavaLocalRef<jobjectArray>
PersonalDataManagerAndroid::GetProfileLabelsForSettings(
    JNIEnv* env,
    const JavaParamRef<jobject>& unused_obj) {
  return GetProfileLabels(env, false /* address_only */,
                          false /* include_name_in_label */,
                          true /* include_organization_in_label */,
                          true /* include_country_in_label */,
                          personal_data_manager_->GetProfilesForSettings());
}

ScopedJavaLocalRef<jobjectArray>
PersonalDataManagerAndroid::GetProfileLabelsToSuggest(
    JNIEnv* env,
    const JavaParamRef<jobject>& unused_obj,
    jboolean include_name_in_label,
    jboolean include_organization_in_label,
    jboolean include_country_in_label) {
  return GetProfileLabels(env, true /* address_only */, include_name_in_label,
                          include_organization_in_label,
                          include_country_in_label,
                          personal_data_manager_->GetProfilesToSuggest());
}

base::android::ScopedJavaLocalRef<jstring>
PersonalDataManagerAndroid::GetShippingAddressLabelForPaymentRequest(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& unused_obj,
    const base::android::JavaParamRef<jobject>& jprofile,
    const JavaParamRef<jstring>& jguid,
    bool include_country_in_label) {
  // The full name is not included in the label for shipping address. It is
  // added separately instead.
  static constexpr ServerFieldType kLabelFields[] = {
      COMPANY_NAME,         ADDRESS_HOME_LINE1,
      ADDRESS_HOME_LINE2,   ADDRESS_HOME_DEPENDENT_LOCALITY,
      ADDRESS_HOME_CITY,    ADDRESS_HOME_STATE,
      ADDRESS_HOME_ZIP,     ADDRESS_HOME_SORTING_CODE,
      ADDRESS_HOME_COUNTRY,
  };
  size_t kLabelFields_size = std::size(kLabelFields);
  if (!include_country_in_label)
    --kLabelFields_size;

  AutofillProfile profile = AutofillProfile::CreateFromJavaObject(
      jprofile,
      personal_data_manager_->GetProfileByGUID(
          ConvertJavaStringToUTF8(env, jguid)),
      g_browser_process->GetApplicationLocale());

  return ConvertUTF16ToJavaString(
      env, profile.ConstructInferredLabel(
               kLabelFields, kLabelFields_size, kLabelFields_size,
               g_browser_process->GetApplicationLocale()));
}

base::android::ScopedJavaLocalRef<jobjectArray>
PersonalDataManagerAndroid::GetCreditCardGUIDsForSettings(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& unused_obj) {
  return GetCreditCardGUIDs(env, personal_data_manager_->GetCreditCards());
}

base::android::ScopedJavaLocalRef<jobjectArray>
PersonalDataManagerAndroid::GetCreditCardGUIDsToSuggest(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& unused_obj) {
  return GetCreditCardGUIDs(env,
                            personal_data_manager_->GetCreditCardsToSuggest());
}

ScopedJavaLocalRef<jobject> PersonalDataManagerAndroid::GetCreditCardByGUID(
    JNIEnv* env,
    const JavaParamRef<jobject>& unused_obj,
    const JavaParamRef<jstring>& jguid) {
  CreditCard* card = personal_data_manager_->GetCreditCardByGUID(
      ConvertJavaStringToUTF8(env, jguid));
  if (!card)
    return ScopedJavaLocalRef<jobject>();

  return PersonalDataManagerAndroid::CreateJavaCreditCardFromNative(env, *card);
}

ScopedJavaLocalRef<jobject> PersonalDataManagerAndroid::GetCreditCardForNumber(
    JNIEnv* env,
    const JavaParamRef<jobject>& unused_obj,
    const JavaParamRef<jstring>& jcard_number) {
  // A local card with empty GUID.
  CreditCard card("", "");
  card.SetNumber(ConvertJavaStringToUTF16(env, jcard_number));
  return PersonalDataManagerAndroid::CreateJavaCreditCardFromNative(env, card);
}

ScopedJavaLocalRef<jstring> PersonalDataManagerAndroid::SetCreditCard(
    JNIEnv* env,
    const JavaParamRef<jobject>& unused_obj,
    const JavaParamRef<jobject>& jcard) {
  std::string guid =
      ConvertJavaStringToUTF8(env, Java_CreditCard_getGUID(env, jcard).obj());

  CreditCard card;
  PopulateNativeCreditCardFromJava(jcard, env, &card);

  if (guid.empty()) {
    personal_data_manager_->AddCreditCard(card);
  } else {
    card.set_guid(guid);
    personal_data_manager_->UpdateCreditCard(card);
  }
  return ConvertUTF8ToJavaString(env, card.guid());
}

void PersonalDataManagerAndroid::UpdateServerCardBillingAddress(
    JNIEnv* env,
    const JavaParamRef<jobject>& unused_obj,
    const JavaParamRef<jobject>& jcard) {
  CreditCard card;
  PopulateNativeCreditCardFromJava(jcard, env, &card);

  personal_data_manager_->UpdateServerCardsMetadata({card});
}

ScopedJavaLocalRef<jstring>
PersonalDataManagerAndroid::GetBasicCardIssuerNetwork(
    JNIEnv* env,
    const JavaParamRef<jobject>& unused_obj,
    const JavaParamRef<jstring>& jcard_number,
    const jboolean jempty_if_invalid) {
  std::u16string card_number = ConvertJavaStringToUTF16(env, jcard_number);

  if (static_cast<bool>(jempty_if_invalid) &&
      !IsValidCreditCardNumber(card_number)) {
    return ConvertUTF8ToJavaString(env, "");
  }
  return ConvertUTF8ToJavaString(
      env,
      data_util::GetPaymentRequestData(CreditCard::GetCardNetwork(card_number))
          .basic_card_issuer_network);
}

void PersonalDataManagerAndroid::AddServerCreditCardForTest(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& unused_obj,
    const base::android::JavaParamRef<jobject>& jcard) {
  std::unique_ptr<CreditCard> card = std::make_unique<CreditCard>();
  PopulateNativeCreditCardFromJava(jcard, env, card.get());
  card->set_record_type(CreditCard::RecordType::kMaskedServerCard);
  personal_data_manager_->AddServerCreditCardForTest(std::move(card));
  personal_data_manager_->NotifyPersonalDataObserver();
}

void PersonalDataManagerAndroid::AddServerCreditCardForTestWithAdditionalFields(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& unused_obj,
    const base::android::JavaParamRef<jobject>& jcard,
    const base::android::JavaParamRef<jstring>& jnickname,
    jint jcard_issuer) {
  std::unique_ptr<CreditCard> card = std::make_unique<CreditCard>();
  PopulateNativeCreditCardFromJava(jcard, env, card.get());
  card->set_record_type(CreditCard::RecordType::kMaskedServerCard);
  card->SetNickname(ConvertJavaStringToUTF16(jnickname));
  card->set_card_issuer(static_cast<CreditCard::Issuer>(jcard_issuer));
  personal_data_manager_->AddServerCreditCardForTest(std::move(card));
  personal_data_manager_->NotifyPersonalDataObserver();
}

void PersonalDataManagerAndroid::RemoveByGUID(
    JNIEnv* env,
    const JavaParamRef<jobject>& unused_obj,
    const JavaParamRef<jstring>& jguid) {
  personal_data_manager_->RemoveByGUID(ConvertJavaStringToUTF8(env, jguid));
}

void PersonalDataManagerAndroid::DeleteAllLocalCreditCards(JNIEnv* env) {
  personal_data_manager_->DeleteAllLocalCreditCards();
}

void PersonalDataManagerAndroid::ClearUnmaskedCache(
    JNIEnv* env,
    const JavaParamRef<jobject>& unused_obj,
    const JavaParamRef<jstring>& guid) {
  personal_data_manager_->ResetFullServerCard(
      ConvertJavaStringToUTF8(env, guid));
}

void PersonalDataManagerAndroid::OnPersonalDataChanged() {
  JNIEnv* env = base::android::AttachCurrentThread();
  auto java_obj = weak_java_obj_.get(env);
  if (java_obj.is_null())
    return;

  Java_PersonalDataManager_personalDataChanged(env, java_obj);
}

void PersonalDataManagerAndroid::RecordAndLogProfileUse(
    JNIEnv* env,
    const JavaParamRef<jobject>& unused_obj,
    const JavaParamRef<jstring>& jguid) {
  AutofillProfile* profile = personal_data_manager_->GetProfileByGUID(
      ConvertJavaStringToUTF8(env, jguid));
  if (profile)
    personal_data_manager_->RecordUseOf(profile);
}

void PersonalDataManagerAndroid::SetProfileUseStatsForTesting(
    JNIEnv* env,
    const JavaParamRef<jobject>& unused_obj,
    const JavaParamRef<jstring>& jguid,
    jint count,
    jint days_since_last_used) {
  DCHECK(count >= 0 && days_since_last_used >= 0);

  AutofillProfile* profile = personal_data_manager_->GetProfileByGUID(
      ConvertJavaStringToUTF8(env, jguid));
  profile->set_use_count(static_cast<size_t>(count));
  profile->set_use_date(AutofillClock::Now() -
                        base::Days(days_since_last_used));

  personal_data_manager_->NotifyPersonalDataObserver();
}

jint PersonalDataManagerAndroid::GetProfileUseCountForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& unused_obj,
    const base::android::JavaParamRef<jstring>& jguid) {
  AutofillProfile* profile = personal_data_manager_->GetProfileByGUID(
      ConvertJavaStringToUTF8(env, jguid));
  return profile->use_count();
}

jlong PersonalDataManagerAndroid::GetProfileUseDateForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& unused_obj,
    const base::android::JavaParamRef<jstring>& jguid) {
  AutofillProfile* profile = personal_data_manager_->GetProfileByGUID(
      ConvertJavaStringToUTF8(env, jguid));
  return profile->use_date().ToTimeT();
}

void PersonalDataManagerAndroid::RecordAndLogCreditCardUse(
    JNIEnv* env,
    const JavaParamRef<jobject>& unused_obj,
    const JavaParamRef<jstring>& jguid) {
  CreditCard* card = personal_data_manager_->GetCreditCardByGUID(
      ConvertJavaStringToUTF8(env, jguid));
  if (card)
    personal_data_manager_->RecordUseOf(card);
}

void PersonalDataManagerAndroid::SetCreditCardUseStatsForTesting(
    JNIEnv* env,
    const JavaParamRef<jobject>& unused_obj,
    const JavaParamRef<jstring>& jguid,
    jint count,
    jint days_since_last_used) {
  DCHECK(count >= 0 && days_since_last_used >= 0);

  CreditCard* card = personal_data_manager_->GetCreditCardByGUID(
      ConvertJavaStringToUTF8(env, jguid));
  card->set_use_count(static_cast<size_t>(count));
  card->set_use_date(AutofillClock::Now() - base::Days(days_since_last_used));

  personal_data_manager_->NotifyPersonalDataObserver();
}

jint PersonalDataManagerAndroid::GetCreditCardUseCountForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& unused_obj,
    const base::android::JavaParamRef<jstring>& jguid) {
  CreditCard* card = personal_data_manager_->GetCreditCardByGUID(
      ConvertJavaStringToUTF8(env, jguid));
  return card->use_count();
}

jlong PersonalDataManagerAndroid::GetCreditCardUseDateForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& unused_obj,
    const base::android::JavaParamRef<jstring>& jguid) {
  CreditCard* card = personal_data_manager_->GetCreditCardByGUID(
      ConvertJavaStringToUTF8(env, jguid));
  return card->use_date().ToTimeT();
}

// TODO(crbug.com/629507): Use a mock clock for testing.
jlong PersonalDataManagerAndroid::GetCurrentDateForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& unused_obj) {
  return base::Time::Now().ToTimeT();
}

jlong PersonalDataManagerAndroid::GetDateNDaysAgoForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& unused_obj,
    jint days) {
  return (AutofillClock::Now() - base::Days(days)).ToTimeT();
}

void PersonalDataManagerAndroid::ClearServerDataForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& unused_obj) {
  personal_data_manager_->ClearAllServerDataForTesting();  // IN-TEST
  personal_data_manager_->NotifyPersonalDataObserver();
}

jboolean PersonalDataManagerAndroid::HasProfiles(JNIEnv* env) {
  return !personal_data_manager_->GetProfiles().empty();
}

jboolean PersonalDataManagerAndroid::HasCreditCards(JNIEnv* env) {
  return !personal_data_manager_->GetCreditCards().empty();
}

jboolean PersonalDataManagerAndroid::IsFidoAuthenticationAvailable(
    JNIEnv* env) {
  // Don't show toggle switch if user is unable to downstream cards.
  if (!personal_data_manager_->IsPaymentsDownloadActive()) {
    return false;
  }
  // Show the toggle switch only if FIDO authentication is available.
  return IsCreditCardFidoAuthenticationEnabled();
}

void PersonalDataManagerAndroid::SetSyncServiceForTesting(JNIEnv* env) {
  personal_data_manager_->SetSyncingForTest(true);
}

base::android::ScopedJavaLocalRef<jobject>
PersonalDataManagerAndroid::GetOrCreateJavaImageFetcher(JNIEnv* env) {
  return static_cast<AutofillImageFetcherImpl*>(
             personal_data_manager_->GetImageFetcher())
      ->GetOrCreateJavaImageFetcher();
}

ScopedJavaLocalRef<jobjectArray> PersonalDataManagerAndroid::GetProfileGUIDs(
    JNIEnv* env,
    const std::vector<AutofillProfile*>& profiles) {
  std::vector<std::u16string> guids;
  for (AutofillProfile* profile : profiles)
    guids.push_back(base::UTF8ToUTF16(profile->guid()));

  return base::android::ToJavaArrayOfStrings(env, guids);
}

ScopedJavaLocalRef<jobjectArray> PersonalDataManagerAndroid::GetCreditCardGUIDs(
    JNIEnv* env,
    const std::vector<CreditCard*>& credit_cards) {
  std::vector<std::u16string> guids;
  for (CreditCard* credit_card : credit_cards)
    guids.push_back(base::UTF8ToUTF16(credit_card->guid()));

  return base::android::ToJavaArrayOfStrings(env, guids);
}

ScopedJavaLocalRef<jobjectArray> PersonalDataManagerAndroid::GetProfileLabels(
    JNIEnv* env,
    bool address_only,
    bool include_name_in_label,
    bool include_organization_in_label,
    bool include_country_in_label,
    std::vector<AutofillProfile*> profiles) {
  ServerFieldTypeSet suggested_fields;
  size_t minimal_fields_shown = 2;
  if (address_only) {
    suggested_fields = ServerFieldTypeSet();
    if (include_name_in_label)
      suggested_fields.insert(NAME_FULL);
    if (include_organization_in_label)
      suggested_fields.insert(COMPANY_NAME);
    suggested_fields.insert(ADDRESS_HOME_LINE1);
    suggested_fields.insert(ADDRESS_HOME_LINE2);
    suggested_fields.insert(ADDRESS_HOME_DEPENDENT_LOCALITY);
    suggested_fields.insert(ADDRESS_HOME_CITY);
    suggested_fields.insert(ADDRESS_HOME_STATE);
    suggested_fields.insert(ADDRESS_HOME_ZIP);
    suggested_fields.insert(ADDRESS_HOME_SORTING_CODE);
    if (include_country_in_label)
      suggested_fields.insert(ADDRESS_HOME_COUNTRY);
    minimal_fields_shown = suggested_fields.size();
  }

  ServerFieldType excluded_field =
      include_name_in_label ? UNKNOWN_TYPE : NAME_FULL;

  std::vector<std::u16string> labels;
  // TODO(crbug.com/1487119): Replace by `profiles` when `GetProfilesToSuggest`
  // starts returning a list of const AutofillProfile*.
  AutofillProfile::CreateInferredLabels(
      std::vector<const AutofillProfile*>(profiles.begin(), profiles.end()),
      address_only ? absl::make_optional(suggested_fields) : absl::nullopt,
      excluded_field, minimal_fields_shown,
      g_browser_process->GetApplicationLocale(), &labels);

  return base::android::ToJavaArrayOfStrings(env, labels);
}

// Returns whether the Autofill feature is managed.
static jboolean JNI_PersonalDataManager_IsAutofillManaged(JNIEnv* env) {
  return prefs::IsAutofillManaged(GetPrefs());
}

// Returns whether the Autofill feature for profiles is managed.
static jboolean JNI_PersonalDataManager_IsAutofillProfileManaged(JNIEnv* env) {
  return prefs::IsAutofillProfileManaged(GetPrefs());
}

// Returns whether the Autofill feature for credit cards is managed.
static jboolean JNI_PersonalDataManager_IsAutofillCreditCardManaged(
    JNIEnv* env) {
  return prefs::IsAutofillCreditCardManaged(GetPrefs());
}

// Returns an ISO 3166-1-alpha-2 country code for a |jcountry_name| using
// the application locale, or an empty string.
static ScopedJavaLocalRef<jstring> JNI_PersonalDataManager_ToCountryCode(
    JNIEnv* env,
    const JavaParamRef<jstring>& jcountry_name) {
  return ConvertUTF8ToJavaString(
      env, CountryNames::GetInstance()->GetCountryCode(
               base::android::ConvertJavaStringToUTF16(env, jcountry_name)));
}

static jlong JNI_PersonalDataManager_Init(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj) {
  PersonalDataManagerAndroid* personal_data_manager_android =
      new PersonalDataManagerAndroid(env, obj);
  return reinterpret_cast<intptr_t>(personal_data_manager_android);
}

}  // namespace autofill
