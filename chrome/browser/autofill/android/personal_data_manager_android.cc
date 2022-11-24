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
#include "base/bind.h"
#include "base/command_line.h"
#include "base/format_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/autofill/address_normalizer_factory.h"
#include "chrome/browser/autofill/android/jni_headers/PersonalDataManager_jni.h"
#include "chrome/browser/autofill/autofill_popup_controller_utils.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/autofill/validation_rules_storage_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/address_normalizer.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/address_i18n.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "components/autofill/core/browser/payments/full_card_request.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/sync_utils.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/libaddressinput/chromium/chrome_metadata_source.h"
#include "third_party/libaddressinput/chromium/chrome_storage_impl.h"
#include "ui/base/l10n/l10n_util.h"
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
using payments::FullCardRequest;

Profile* GetProfile() {
  return ProfileManager::GetActiveUserProfile()->GetOriginalProfile();
}

PrefService* GetPrefs() {
  return GetProfile()->GetPrefs();
}

void MaybeSetRawInfoWithVerificationStatus(
    AutofillProfile* profile,
    ServerFieldType type,
    const base::android::JavaRef<jstring>& value,
    jint status) {
  if (value)
    profile->SetRawInfoWithVerificationStatus(
        type, ConvertJavaStringToUTF16(value),
        static_cast<structured_address::VerificationStatus>(status));
}

void MaybeSetInfoWithVerificationStatus(
    AutofillProfile* profile,
    ServerFieldType type,
    const base::android::JavaRef<jstring>& value,
    jint status) {
  if (value)
    profile->SetInfoWithVerificationStatus(
        type, ConvertJavaStringToUTF16(value),
        g_browser_process->GetApplicationLocale(),
        static_cast<structured_address::VerificationStatus>(status));
}

// Self-deleting requester of full card details, including full PAN and the CVC
// number.
class FullCardRequester : public FullCardRequest::ResultDelegate,
                          public base::SupportsWeakPtr<FullCardRequester> {
 public:
  FullCardRequester() {}

  FullCardRequester(const FullCardRequester&) = delete;
  FullCardRequester& operator=(const FullCardRequester&) = delete;

  // Takes ownership of |card|.
  void GetFullCard(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& jweb_contents,
                   const base::android::JavaParamRef<jobject>& jdelegate,
                   std::unique_ptr<CreditCard> card) {
    card_ = std::move(card);
    jdelegate_.Reset(env, jdelegate);

    if (!card_) {
      OnFullCardRequestFailed(card_->record_type(),
                              FullCardRequest::FailureType::GENERIC_FAILURE);
      return;
    }

    content::WebContents* contents =
        content::WebContents::FromJavaWebContents(jweb_contents);
    if (!contents) {
      OnFullCardRequestFailed(card_->record_type(),
                              FullCardRequest::FailureType::GENERIC_FAILURE);
      return;
    }

    ContentAutofillDriverFactory* factory =
        ContentAutofillDriverFactory::FromWebContents(contents);
    if (!factory) {
      OnFullCardRequestFailed(card_->record_type(),
                              FullCardRequest::FailureType::GENERIC_FAILURE);
      return;
    }

    ContentAutofillDriver* driver =
        factory->DriverForFrame(contents->GetPrimaryMainFrame());
    if (!driver) {
      OnFullCardRequestFailed(card_->record_type(),
                              FullCardRequest::FailureType::GENERIC_FAILURE);
      return;
    }

    CreditCardCVCAuthenticator* cvc_authenticator =
        driver->autofill_manager()->client()->GetCVCAuthenticator();
    cvc_authenticator->GetFullCardRequest()->GetFullCard(
        *card_, AutofillClient::UnmaskCardReason::kPaymentRequest, AsWeakPtr(),
        cvc_authenticator->GetAsFullCardRequestUIDelegate());
  }

 private:
  ~FullCardRequester() override {}

  // payments::FullCardRequest::ResultDelegate:
  void OnFullCardRequestSucceeded(
      const payments::FullCardRequest& /* full_card_request */,
      const CreditCard& card,
      const std::u16string& cvc) override {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_FullCardRequestDelegate_onFullCardDetails(
        env, jdelegate_,
        PersonalDataManagerAndroid::CreateJavaCreditCardFromNative(env, card),
        base::android::ConvertUTF16ToJavaString(env, cvc));
    delete this;
  }

  // payments::FullCardRequest::ResultDelegate:
  void OnFullCardRequestFailed(
      CreditCard::RecordType card_type,
      FullCardRequest::FailureType failure_type) override {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_FullCardRequestDelegate_onFullCardError(env, jdelegate_);
    delete this;
  }

  std::unique_ptr<CreditCard> card_;
  ScopedJavaGlobalRef<jobject> jdelegate_;
};

void OnSubKeysReceived(ScopedJavaGlobalRef<jobject> jdelegate,
                       const std::vector<std::string>& subkeys_codes,
                       const std::vector<std::string>& subkeys_names) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_GetSubKeysRequestDelegate_onSubKeysReceived(
      env, jdelegate, base::android::ToJavaArrayOfStrings(env, subkeys_codes),
      base::android::ToJavaArrayOfStrings(env, subkeys_names));
}

void OnAddressNormalized(ScopedJavaGlobalRef<jobject> jdelegate,
                         bool success,
                         const AutofillProfile& profile) {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (success) {
    Java_NormalizedAddressRequestDelegate_onAddressNormalized(
        env, jdelegate,
        PersonalDataManagerAndroid::CreateJavaProfileFromNative(env, profile));
  } else {
    Java_NormalizedAddressRequestDelegate_onCouldNotNormalize(
        env, jdelegate,
        PersonalDataManagerAndroid::CreateJavaProfileFromNative(env, profile));
  }
}

}  // namespace

PersonalDataManagerAndroid::PersonalDataManagerAndroid(JNIEnv* env, jobject obj)
    : weak_java_obj_(env, obj),
      personal_data_manager_(PersonalDataManagerFactory::GetForProfile(
          ProfileManager::GetActiveUserProfile())),
      subkey_requester_(std::make_unique<ChromeMetadataSource>(
                            I18N_ADDRESS_VALIDATION_DATA_URL,
                            g_browser_process->system_network_context_manager()
                                ->GetSharedURLLoaderFactory()),
                        ValidationRulesStorageFactory::CreateStorage()) {
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
      card.record_type() == CreditCard::LOCAL_CARD,
      card.record_type() == CreditCard::FULL_SERVER_CARD,
      ConvertUTF16ToJavaString(env, card.GetRawInfo(CREDIT_CARD_NAME_FULL)),
      ConvertUTF16ToJavaString(env, card.GetRawInfo(CREDIT_CARD_NUMBER)),
      ConvertUTF16ToJavaString(env, card.NetworkAndLastFourDigits()),
      ConvertUTF16ToJavaString(env, card.GetRawInfo(CREDIT_CARD_EXP_MONTH)),
      ConvertUTF16ToJavaString(env,
                               card.GetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR)),
      ConvertUTF8ToJavaString(env,
                              payment_request_data.basic_card_issuer_network),
      ResourceMapper::MapToJavaDrawableId(
          GetIconResourceID(card.CardIconStringForAutofillSuggestion())),
      ConvertUTF8ToJavaString(env, card.billing_address_id()),
      ConvertUTF8ToJavaString(env, card.server_id()), card.instrument_id(),
      ConvertUTF16ToJavaString(env,
                               card.CardIdentifierStringForAutofillDisplay()),
      ConvertUTF16ToJavaString(env, card.nickname()),
      url::GURLAndroid::FromNativeGURL(env, card.card_art_url()),
      static_cast<jint>(card.virtual_card_enrollment_state()),
      ConvertUTF16ToJavaString(env, card.product_description()),
      ConvertUTF16ToJavaString(env, card.CardNameForAutofillDisplay()),
      ConvertUTF16ToJavaString(env, card.ObfuscatedLastFourDigits()));
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
    card->set_record_type(CreditCard::LOCAL_CARD);
  } else {
    if (Java_CreditCard_getIsCached(env, jcard)) {
      card->set_record_type(CreditCard::FULL_SERVER_CARD);
    } else {
      card->set_record_type(CreditCard::MASKED_SERVER_CARD);
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
}

// static
ScopedJavaLocalRef<jobject>
PersonalDataManagerAndroid::CreateJavaProfileFromNative(
    JNIEnv* env,
    const AutofillProfile& profile) {
  return Java_AutofillProfile_create(
      env, ConvertUTF8ToJavaString(env, profile.guid()),
      ConvertUTF8ToJavaString(env, profile.origin()),
      profile.record_type() == AutofillProfile::LOCAL_PROFILE,
      ConvertUTF16ToJavaString(
          env, profile.GetInfo(AutofillType(NAME_HONORIFIC_PREFIX),
                               g_browser_process->GetApplicationLocale())),
      static_cast<jint>(profile.GetVerificationStatus(NAME_HONORIFIC_PREFIX)),
      ConvertUTF16ToJavaString(
          env, profile.GetInfo(AutofillType(NAME_FULL),
                               g_browser_process->GetApplicationLocale())),
      static_cast<jint>(profile.GetVerificationStatus(NAME_FULL)),
      ConvertUTF16ToJavaString(env, profile.GetRawInfo(COMPANY_NAME)),
      static_cast<jint>(profile.GetVerificationStatus(COMPANY_NAME)),
      ConvertUTF16ToJavaString(env,
                               profile.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS)),
      static_cast<jint>(
          profile.GetVerificationStatus(ADDRESS_HOME_STREET_ADDRESS)),
      ConvertUTF16ToJavaString(env, profile.GetRawInfo(ADDRESS_HOME_STATE)),
      static_cast<jint>(profile.GetVerificationStatus(ADDRESS_HOME_STATE)),
      ConvertUTF16ToJavaString(env, profile.GetRawInfo(ADDRESS_HOME_CITY)),
      static_cast<jint>(profile.GetVerificationStatus(ADDRESS_HOME_CITY)),
      ConvertUTF16ToJavaString(
          env, profile.GetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY)),
      static_cast<jint>(
          profile.GetVerificationStatus(ADDRESS_HOME_DEPENDENT_LOCALITY)),
      ConvertUTF16ToJavaString(env, profile.GetRawInfo(ADDRESS_HOME_ZIP)),
      static_cast<jint>(profile.GetVerificationStatus(ADDRESS_HOME_ZIP)),
      ConvertUTF16ToJavaString(env,
                               profile.GetRawInfo(ADDRESS_HOME_SORTING_CODE)),
      static_cast<jint>(
          profile.GetVerificationStatus(ADDRESS_HOME_SORTING_CODE)),
      ConvertUTF16ToJavaString(env, profile.GetRawInfo(ADDRESS_HOME_COUNTRY)),
      static_cast<jint>(profile.GetVerificationStatus(ADDRESS_HOME_COUNTRY)),
      ConvertUTF16ToJavaString(env,
                               profile.GetRawInfo(PHONE_HOME_WHOLE_NUMBER)),
      static_cast<jint>(profile.GetVerificationStatus(PHONE_HOME_WHOLE_NUMBER)),
      ConvertUTF16ToJavaString(env, profile.GetRawInfo(EMAIL_ADDRESS)),
      static_cast<jint>(profile.GetVerificationStatus(EMAIL_ADDRESS)),
      ConvertUTF8ToJavaString(env, profile.language_code()));
}

// static
void PersonalDataManagerAndroid::PopulateNativeProfileFromJava(
    const JavaParamRef<jobject>& jprofile,
    JNIEnv* env,
    AutofillProfile* profile) {
  // Only set the guid if it is an existing profile (java guid not empty).
  // Otherwise, keep the generated one.
  std::string guid =
      ConvertJavaStringToUTF8(Java_AutofillProfile_getGUID(env, jprofile));
  if (!guid.empty())
    profile->set_guid(guid);

  profile->set_origin(
      ConvertJavaStringToUTF8(Java_AutofillProfile_getOrigin(env, jprofile)));
  MaybeSetInfoWithVerificationStatus(
      profile, NAME_FULL, Java_AutofillProfile_getFullName(env, jprofile),
      Java_AutofillProfile_getFullNameStatus(env, jprofile));
  MaybeSetRawInfoWithVerificationStatus(
      profile, NAME_HONORIFIC_PREFIX,
      Java_AutofillProfile_getHonorificPrefix(env, jprofile),
      Java_AutofillProfile_getHonorificPrefixStatus(env, jprofile));
  MaybeSetRawInfoWithVerificationStatus(
      profile, COMPANY_NAME, Java_AutofillProfile_getCompanyName(env, jprofile),
      Java_AutofillProfile_getCompanyNameStatus(env, jprofile));
  MaybeSetRawInfoWithVerificationStatus(
      profile, ADDRESS_HOME_STREET_ADDRESS,
      Java_AutofillProfile_getStreetAddress(env, jprofile),
      Java_AutofillProfile_getStreetAddressStatus(env, jprofile));
  MaybeSetRawInfoWithVerificationStatus(
      profile, ADDRESS_HOME_STATE,
      Java_AutofillProfile_getRegion(env, jprofile),
      Java_AutofillProfile_getRegionStatus(env, jprofile));
  MaybeSetRawInfoWithVerificationStatus(
      profile, ADDRESS_HOME_CITY,
      Java_AutofillProfile_getLocality(env, jprofile),
      Java_AutofillProfile_getLocalityStatus(env, jprofile));
  MaybeSetRawInfoWithVerificationStatus(
      profile, ADDRESS_HOME_DEPENDENT_LOCALITY,
      Java_AutofillProfile_getDependentLocality(env, jprofile),
      Java_AutofillProfile_getDependentLocalityStatus(env, jprofile));
  MaybeSetRawInfoWithVerificationStatus(
      profile, ADDRESS_HOME_ZIP,
      Java_AutofillProfile_getPostalCode(env, jprofile),
      Java_AutofillProfile_getPostalCodeStatus(env, jprofile));
  MaybeSetRawInfoWithVerificationStatus(
      profile, ADDRESS_HOME_SORTING_CODE,
      Java_AutofillProfile_getSortingCode(env, jprofile),
      Java_AutofillProfile_getSortingCodeStatus(env, jprofile));
  MaybeSetInfoWithVerificationStatus(
      profile, ADDRESS_HOME_COUNTRY,
      Java_AutofillProfile_getCountryCode(env, jprofile),
      Java_AutofillProfile_getCountryCodeStatus(env, jprofile));
  MaybeSetRawInfoWithVerificationStatus(
      profile, PHONE_HOME_WHOLE_NUMBER,
      Java_AutofillProfile_getPhoneNumber(env, jprofile),
      Java_AutofillProfile_getPhoneNumberStatus(env, jprofile));
  MaybeSetRawInfoWithVerificationStatus(
      profile, EMAIL_ADDRESS,
      Java_AutofillProfile_getEmailAddress(env, jprofile),
      Java_AutofillProfile_getEmailAddressStatus(env, jprofile));
  profile->set_language_code(ConvertJavaStringToUTF8(
      Java_AutofillProfile_getLanguageCode(env, jprofile)));
  profile->FinalizeAfterImport();
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
  return GetProfileGUIDs(env, personal_data_manager_->GetProfiles());
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

  return PersonalDataManagerAndroid::CreateJavaProfileFromNative(env, *profile);
}

ScopedJavaLocalRef<jstring> PersonalDataManagerAndroid::SetProfile(
    JNIEnv* env,
    const JavaParamRef<jobject>& unused_obj,
    const JavaParamRef<jobject>& jprofile) {
  std::string guid = ConvertJavaStringToUTF8(
      env, Java_AutofillProfile_getGUID(env, jprofile).obj());

  AutofillProfile profile;
  PopulateNativeProfileFromJava(jprofile, env, &profile);

  if (guid.empty()) {
    personal_data_manager_->AddProfile(profile);
  } else {
    profile.set_guid(guid);
    personal_data_manager_->UpdateProfile(profile);
  }

  return ConvertUTF8ToJavaString(env, profile.guid());
}

ScopedJavaLocalRef<jstring> PersonalDataManagerAndroid::SetProfileToLocal(
    JNIEnv* env,
    const JavaParamRef<jobject>& unused_obj,
    const JavaParamRef<jobject>& jprofile) {
  AutofillProfile profile;
  PopulateNativeProfileFromJava(jprofile, env, &profile);

  AutofillProfile* target_profile =
      personal_data_manager_->GetProfileByGUID(ConvertJavaStringToUTF8(
          env, Java_AutofillProfile_getGUID(env, jprofile).obj()));

  if (target_profile != nullptr &&
      target_profile->record_type() == AutofillProfile::LOCAL_PROFILE) {
    profile.set_guid(target_profile->guid());
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
                          personal_data_manager_->GetProfiles());
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
PersonalDataManagerAndroid::GetShippingAddressLabelWithCountryForPaymentRequest(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& unused_obj,
    const base::android::JavaParamRef<jobject>& jprofile) {
  return GetShippingAddressLabelForPaymentRequest(
      env, jprofile, true /* include_country_in_label */);
}

base::android::ScopedJavaLocalRef<jstring> PersonalDataManagerAndroid::
    GetShippingAddressLabelWithoutCountryForPaymentRequest(
        JNIEnv* env,
        const base::android::JavaParamRef<jobject>& unused_obj,
        const base::android::JavaParamRef<jobject>& jprofile) {
  return GetShippingAddressLabelForPaymentRequest(
      env, jprofile, false /* include_country_in_label */);
}

base::android::ScopedJavaLocalRef<jstring>
PersonalDataManagerAndroid::GetBillingAddressLabelForPaymentRequest(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& unused_obj,
    const base::android::JavaParamRef<jobject>& jprofile) {
  // The company name and country are not included in the billing address label.
  static constexpr ServerFieldType kLabelFields[] = {
      NAME_FULL,          ADDRESS_HOME_LINE1,
      ADDRESS_HOME_LINE2, ADDRESS_HOME_DEPENDENT_LOCALITY,
      ADDRESS_HOME_CITY,  ADDRESS_HOME_STATE,
      ADDRESS_HOME_ZIP,   ADDRESS_HOME_SORTING_CODE,
  };

  AutofillProfile profile;
  PopulateNativeProfileFromJava(jprofile, env, &profile);

  return ConvertUTF16ToJavaString(
      env, profile.ConstructInferredLabel(
               kLabelFields, std::size(kLabelFields), std::size(kLabelFields),
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
    const base::android::JavaParamRef<jobject>& unused_obj,
    bool include_server_cards) {
  return GetCreditCardGUIDs(
      env,
      personal_data_manager_->GetCreditCardsToSuggest(include_server_cards));
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
  card->set_record_type(CreditCard::MASKED_SERVER_CARD);
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
  card->set_record_type(CreditCard::MASKED_SERVER_CARD);
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

void PersonalDataManagerAndroid::ClearUnmaskedCache(
    JNIEnv* env,
    const JavaParamRef<jobject>& unused_obj,
    const JavaParamRef<jstring>& guid) {
  personal_data_manager_->ResetFullServerCard(
      ConvertJavaStringToUTF8(env, guid));
}

void PersonalDataManagerAndroid::GetFullCardForPaymentRequest(
    JNIEnv* env,
    const JavaParamRef<jobject>& unused_obj,
    const JavaParamRef<jobject>& jweb_contents,
    const JavaParamRef<jobject>& jcard,
    const JavaParamRef<jobject>& jdelegate) {
  std::unique_ptr<CreditCard> card = std::make_unique<CreditCard>();
  PopulateNativeCreditCardFromJava(jcard, env, card.get());
  // Self-deleting object.
  (new FullCardRequester())
      ->GetFullCard(env, jweb_contents, jdelegate, std::move(card));
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
    jint date) {
  DCHECK(count >= 0 && date >= 0);

  AutofillProfile* profile = personal_data_manager_->GetProfileByGUID(
      ConvertJavaStringToUTF8(env, jguid));
  profile->set_use_count(static_cast<size_t>(count));
  profile->set_use_date(base::Time::FromTimeT(date));

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
    jint date) {
  DCHECK(count >= 0 && date >= 0);

  CreditCard* card = personal_data_manager_->GetCreditCardByGUID(
      ConvertJavaStringToUTF8(env, jguid));
  card->set_use_count(static_cast<size_t>(count));
  card->set_use_date(base::Time::FromTimeT(date));

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

void PersonalDataManagerAndroid::ClearServerDataForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& unused_obj) {
  personal_data_manager_->ClearAllServerData();
  personal_data_manager_->NotifyPersonalDataObserver();
}

void PersonalDataManagerAndroid::LoadRulesForAddressNormalization(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& unused_obj,
    const base::android::JavaParamRef<jstring>& jregion_code) {
  AddressNormalizer* normalizer = AddressNormalizerFactory::GetInstance();
  normalizer->LoadRulesForRegion(ConvertJavaStringToUTF8(env, jregion_code));
}

void PersonalDataManagerAndroid::LoadRulesForSubKeys(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& unused_obj,
    const base::android::JavaParamRef<jstring>& jregion_code) {
  subkey_requester_.LoadRulesForRegion(
      ConvertJavaStringToUTF8(env, jregion_code));
}

void PersonalDataManagerAndroid::StartAddressNormalization(
    JNIEnv* env,
    const JavaParamRef<jobject>& unused_obj,
    const JavaParamRef<jobject>& jprofile,
    jint jtimeout_seconds,
    const JavaParamRef<jobject>& jdelegate) {
  AutofillProfile profile;
  PopulateNativeProfileFromJava(jprofile, env, &profile);

  // Start the normalization.
  AddressNormalizer* normalizer = AddressNormalizerFactory::GetInstance();
  normalizer->NormalizeAddressAsync(
      profile, jtimeout_seconds,
      base::BindOnce(&OnAddressNormalized,
                     ScopedJavaGlobalRef<jobject>(jdelegate)));
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
  if (personal_data_manager_->GetSyncSigninState() !=
          AutofillSyncSigninState::kSignedInAndWalletSyncTransportEnabled &&
      personal_data_manager_->GetSyncSigninState() !=
          AutofillSyncSigninState::kSignedInAndSyncFeatureEnabled) {
    return false;
  }
  // Show the toggle switch only if FIDO authentication is available.
  return IsCreditCardFidoAuthenticationEnabled();
}

void PersonalDataManagerAndroid::StartRegionSubKeysRequest(
    JNIEnv* env,
    const JavaParamRef<jobject>& unused_obj,
    const JavaParamRef<jstring>& jregion_code,
    jint jtimeout_seconds,
    const JavaParamRef<jobject>& jdelegate) {
  const std::string region_code = ConvertJavaStringToUTF8(env, jregion_code);

  ScopedJavaGlobalRef<jobject> my_jdelegate;
  my_jdelegate.Reset(env, jdelegate);

  SubKeyReceiverCallback cb = base::BindOnce(
      &OnSubKeysReceived, ScopedJavaGlobalRef<jobject>(my_jdelegate));

  std::string language =
      l10n_util::GetLanguage(g_browser_process->GetApplicationLocale());
  subkey_requester_.StartRegionSubKeysRequest(region_code, language,
                                              jtimeout_seconds, std::move(cb));
}

void PersonalDataManagerAndroid::CancelPendingGetSubKeys(JNIEnv* env) {
  subkey_requester_.CancelPendingGetSubKeys();
}

void PersonalDataManagerAndroid::SetSyncServiceForTesting(JNIEnv* env) {
  personal_data_manager_->SetSyncingForTest(true);
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
  std::unique_ptr<std::vector<ServerFieldType>> suggested_fields;
  size_t minimal_fields_shown = 2;
  if (address_only) {
    suggested_fields = std::make_unique<std::vector<ServerFieldType>>();
    if (include_name_in_label)
      suggested_fields->push_back(NAME_FULL);
    if (include_organization_in_label)
      suggested_fields->push_back(COMPANY_NAME);
    suggested_fields->push_back(ADDRESS_HOME_LINE1);
    suggested_fields->push_back(ADDRESS_HOME_LINE2);
    suggested_fields->push_back(ADDRESS_HOME_DEPENDENT_LOCALITY);
    suggested_fields->push_back(ADDRESS_HOME_CITY);
    suggested_fields->push_back(ADDRESS_HOME_STATE);
    suggested_fields->push_back(ADDRESS_HOME_ZIP);
    suggested_fields->push_back(ADDRESS_HOME_SORTING_CODE);
    if (include_country_in_label)
      suggested_fields->push_back(ADDRESS_HOME_COUNTRY);
    minimal_fields_shown = suggested_fields->size();
  }

  ServerFieldType excluded_field =
      include_name_in_label ? UNKNOWN_TYPE : NAME_FULL;

  std::vector<std::u16string> labels;
  AutofillProfile::CreateInferredLabels(
      profiles, suggested_fields.get(), excluded_field, minimal_fields_shown,
      g_browser_process->GetApplicationLocale(), &labels);

  return base::android::ToJavaArrayOfStrings(env, labels);
}

base::android::ScopedJavaLocalRef<jstring>
PersonalDataManagerAndroid::GetShippingAddressLabelForPaymentRequest(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jprofile,
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

  AutofillProfile profile;
  PopulateNativeProfileFromJava(jprofile, env, &profile);

  return ConvertUTF16ToJavaString(
      env, profile.ConstructInferredLabel(
               kLabelFields, kLabelFields_size, kLabelFields_size,
               g_browser_process->GetApplicationLocale()));
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

// Returns whether the Payments integration feature is enabled.
static jboolean JNI_PersonalDataManager_IsPaymentsIntegrationEnabled(
    JNIEnv* env) {
  return prefs::IsPaymentsIntegrationEnabled(GetPrefs());
}

// Enables or disables the Payments integration feature.
static void JNI_PersonalDataManager_SetPaymentsIntegrationEnabled(
    JNIEnv* env,
    jboolean enable) {
  prefs::SetPaymentsIntegrationEnabled(GetPrefs(), enable);
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
