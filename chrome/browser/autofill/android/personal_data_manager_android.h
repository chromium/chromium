// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ANDROID_PERSONAL_DATA_MANAGER_ANDROID_H_
#define CHROME_BROWSER_AUTOFILL_ANDROID_PERSONAL_DATA_MANAGER_ANDROID_H_

#include <string>
#include <vector>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "components/autofill/core/browser/geo/subkey_requester.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"

namespace autofill {

// Android wrapper of the PersonalDataManager which provides access from the
// Java layer. Note that on Android, there's only a single profile, and
// therefore a single instance of this wrapper.
class PersonalDataManagerAndroid : public PersonalDataManagerObserver {
 public:
  PersonalDataManagerAndroid(JNIEnv* env, jobject obj);

  static base::android::ScopedJavaLocalRef<jobject>
  CreateJavaCreditCardFromNative(JNIEnv* env, const CreditCard& card);
  static void PopulateNativeCreditCardFromJava(
      const base::android::JavaRef<jobject>& jcard,
      JNIEnv* env,
      CreditCard* card);
  static base::android::ScopedJavaLocalRef<jobject> CreateJavaProfileFromNative(
      JNIEnv* env,
      const AutofillProfile& profile);
  static void PopulateNativeProfileFromJava(
      const base::android::JavaParamRef<jobject>& jprofile,
      JNIEnv* env,
      AutofillProfile* profile);

  // Returns true if personal data manager has loaded the initial data.
  jboolean IsDataLoaded(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj) const;

  // These functions act on "web profiles" aka "LOCAL_PROFILE" profiles.
  // -------------------------

  // Returns the GUIDs of all profiles.
  base::android::ScopedJavaLocalRef<jobjectArray> GetProfileGUIDsForSettings(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj);

  // Returns the GUIDs of the profiles to suggest to the user. See
  // PersonalDataManager::GetProfilesToSuggest for more details.
  base::android::ScopedJavaLocalRef<jobjectArray> GetProfileGUIDsToSuggest(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj);

  // Returns the profile with the specified |jguid|, or NULL if there is no
  // profile with the specified |jguid|. Both web and auxiliary profiles may
  // be returned.
  base::android::ScopedJavaLocalRef<jobject> GetProfileByGUID(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jstring>& jguid);

  // Adds or modifies a profile.  If |jguid| is an empty string, we are creating
  // a new profile.  Else we are updating an existing profile.  Always returns
  // the GUID for this profile; the GUID it may have just been created.
  base::android::ScopedJavaLocalRef<jstring> SetProfile(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jobject>& jprofile);

  // Adds or modifies a profile like SetProfile interface if |jprofile| is
  // local. Otherwise it creates a local copy of it.
  base::android::ScopedJavaLocalRef<jstring> SetProfileToLocal(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jobject>& jprofile);

  // Gets the labels for all known profiles. These labels are useful for
  // distinguishing the profiles from one another.
  //
  // The labels never contain the full name and include at least 2 fields.
  base::android::ScopedJavaLocalRef<jobjectArray> GetProfileLabelsForSettings(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj);

  // Gets the labels for the profiles to suggest to the user. These labels are
  // useful for distinguishing the profiles from one another.
  //
  // The labels never contain the email address, or phone numbers. The
  // |include_name_in_label| argument controls whether the name is included.
  // All other fields are included in the label.
  base::android::ScopedJavaLocalRef<jobjectArray> GetProfileLabelsToSuggest(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      jboolean include_name_in_label,
      jboolean include_organization_in_label,
      jboolean include_country_in_label);

  // Returns the shipping label of the given profile for PaymentRequest. This
  // label does not contain the full name or the email address. All other fields
  // are included in the label.
  base::android::ScopedJavaLocalRef<jstring>
  GetShippingAddressLabelWithCountryForPaymentRequest(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jobject>& jprofile);

  // Returns the shipping label of the given profile for PaymentRequest. This
  // label does not contain the full name, the email address or the country. All
  // other fields are included in the label.
  base::android::ScopedJavaLocalRef<jstring>
  GetShippingAddressLabelWithoutCountryForPaymentRequest(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jobject>& jprofile);

  // Returns the billing label of the given profile for PaymentRequest. This
  // label does not contain the company name, the phone number, the country or
  // the email address. All other fields are included in the label.
  base::android::ScopedJavaLocalRef<jstring>
  GetBillingAddressLabelForPaymentRequest(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jobject>& jprofile);

  // These functions act on local credit cards.
  // --------------------

  // Returns the GUIDs of all the credit cards.
  base::android::ScopedJavaLocalRef<jobjectArray> GetCreditCardGUIDsForSettings(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj);

  // Returns the GUIDs of the credit cards to suggest to the user. See
  // PersonalDataManager::GetCreditCardsToSuggest for more details.
  base::android::ScopedJavaLocalRef<jobjectArray> GetCreditCardGUIDsToSuggest(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      bool include_server_cards);

  // Returns the credit card with the specified |jguid|, or NULL if there is
  // no credit card with the specified |jguid|.
  base::android::ScopedJavaLocalRef<jobject> GetCreditCardByGUID(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jstring>& jguid);

  // Returns a credit card with the specified |jcard_number|. This is used for
  // determining the card's obfuscated number, issuer icon, and type in one go.
  // This function does not interact with the autofill table on disk, so can be
  // used for cards that are not saved.
  base::android::ScopedJavaLocalRef<jobject> GetCreditCardForNumber(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jstring>& jcard_number);

  // Adds or modifies a local credit card.  If |jguid| is an empty string, we
  // are creating a new card.  Else we are updating an existing profile.  Always
  // returns the GUID for this profile; the GUID it may have just been created.
  base::android::ScopedJavaLocalRef<jstring> SetCreditCard(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jobject>& jcard);

  // Updates the billing address of a server credit card |jcard|.
  void UpdateServerCardBillingAddress(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jobject>& jcard);

  // Returns the issuer network string according to PaymentRequest spec, or an
  // empty string if the given card number is not valid and |jempty_if_invalid|
  // is true.
  base::android::ScopedJavaLocalRef<jstring> GetBasicCardIssuerNetwork(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jstring>& jcard_number,
      const jboolean jempty_if_invalid);

  // Adds a server credit card. Used only in tests.
  void AddServerCreditCardForTest(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jobject>& jcard);

  // Adds a server credit card and sets the additional fields, for example,
  // card_issuer, nickname. Used only in tests.
  void AddServerCreditCardForTestWithAdditionalFields(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jobject>& jcard,
      const base::android::JavaParamRef<jstring>& jnickname,
      jint jcard_issuer);

  // Removes the profile or credit card represented by |jguid|.
  void RemoveByGUID(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& unused_obj,
                    const base::android::JavaParamRef<jstring>& jguid);

  // Resets the given unmasked card back to the masked state.
  void ClearUnmaskedCache(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jstring>& jguid);

  // Gets the card CVC and expiration date (if it's expired). If the card is
  // masked, unmasks it. If the user has entered new expiration date, the new
  // date is saved on disk.
  //
  // The full card details are sent to the delegate.
  void GetFullCardForPaymentRequest(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jobject>& jweb_contents,
      const base::android::JavaParamRef<jobject>& jcard,
      const base::android::JavaParamRef<jobject>& jdelegate);

  // PersonalDataManagerObserver:
  void OnPersonalDataChanged() override;

  // These functions act on the usage stats of local profiles and credit cards.
  // --------------------

  // Records the use and log usage metrics for the profile associated with the
  // |jguid|. Increments the use count of the profile and sets its use date to
  // the current time.
  void RecordAndLogProfileUse(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jstring>& jguid);

  // Sets the use count and use date of the profile associated to the |jguid|.
  // Both |count| and |date| should be non-negative. |date| represents an
  // absolute point in coordinated universal time (UTC) represented as
  // microseconds since the Windows epoch. For more details see the comment
  // header in time.h.
  void SetProfileUseStatsForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jstring>& jguid,
      jint count,
      jint date);

  // Returns the use count of the profile associated to the |jguid|.
  jint GetProfileUseCountForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jstring>& jguid);

  // Returns the use date of the profile associated to the |jguid|. It
  // represents an absolute point in coordinated universal time (UTC)
  // represented as microseconds since the Windows epoch. For more details see
  // the comment header in time.h.
  jlong GetProfileUseDateForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jstring>& jguid);

  // Records the use and log usage metrics for the credit card associated with
  // the |jguid|. Increments the use count of the credit card and sets its use
  // date to the current time.
  void RecordAndLogCreditCardUse(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jstring>& jguid);

  // Sets the use count and use date of the credit card associated to the
  // |jguid|. Both |count| and |date| should be non-negative. |date| represents
  // an absolute point in coordinated universal time (UTC) represented as
  // microseconds since the Windows epoch. For more details see the comment
  // header in time.h.
  void SetCreditCardUseStatsForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jstring>& jguid,
      jint count,
      jint date);

  // Returns the use count of the credit card associated to the |jguid|.
  jint GetCreditCardUseCountForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jstring>& jguid);

  // Returns the use date of the credit card associated to the |jguid|. It
  // represents an absolute point in coordinated universal time (UTC)
  // represented as microseconds since the Windows epoch. For more details see
  // the comment header in time.h.
  jlong GetCreditCardUseDateForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jstring>& jguid);

  // Returns the current date represented as an absolute point in coordinated
  // universal time (UTC) represented as microseconds since the Unix epoch. For
  // more details see the comment header in time.h
  jlong GetCurrentDateForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj);

  // These functions help address normalization.
  // --------------------

  // Starts loading the address validation rules for the specified
  // |region_code|.
  void LoadRulesForAddressNormalization(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jstring>& region_code);

  // Starts loading the rules for the specified |region_code| for the further
  // subkey request.
  void LoadRulesForSubKeys(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jstring>& region_code);

  // Normalizes the address of the |jprofile| synchronously if the region rules
  // have finished loading. Otherwise sets up the task to start the address
  // normalization when the rules have finished loading. Also defines a time
  // limit for the normalization, in which case the the |jdelegate| will be
  // notified. If the rules are loaded before the timeout, |jdelegate| will
  // receive the normalized profile.
  void StartAddressNormalization(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jobject>& jprofile,
      jint jtimeout_seconds,
      const base::android::JavaParamRef<jobject>& jdelegate);

  // Checks whether the Autofill PersonalDataManager has profiles.
  jboolean HasProfiles(JNIEnv* env);

  // Checks whether the Autofill PersonalDataManager has credit cards.
  jboolean HasCreditCards(JNIEnv* env);

  // Checks whether FIDO authentication is available.
  jboolean IsFidoAuthenticationAvailable(JNIEnv* env);

  // Gets the subkeys for the region with |jregion_code| code, if the
  // |jregion_code| rules have finished loading. Otherwise, sets up a task to
  // get the subkeys, when the rules are loaded.
  void StartRegionSubKeysRequest(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jstring>& jregion_code,
      jint jtimeout_seconds,
      const base::android::JavaParamRef<jobject>& jdelegate);

  // Cancels the pending subkey request task.
  void CancelPendingGetSubKeys(JNIEnv* env);

  void SetSyncServiceForTesting(JNIEnv* env);

 private:
  ~PersonalDataManagerAndroid() override;

  // Returns the GUIDs of the |profiles| passed as parameter.
  base::android::ScopedJavaLocalRef<jobjectArray> GetProfileGUIDs(
      JNIEnv* env,
      const std::vector<AutofillProfile*>& profiles);

  // Returns the GUIDs of the |credit_cards| passed as parameter.
  base::android::ScopedJavaLocalRef<jobjectArray> GetCreditCardGUIDs(
      JNIEnv* env,
      const std::vector<CreditCard*>& credit_cards);

  // Gets the labels for the |profiles| passed as parameters. These labels are
  // useful for distinguishing the profiles from one another.
  //
  // The labels never contain the full name and include at least 2 fields.
  //
  // If |address_only| is true, then such fields as phone number, and email
  // address are also omitted, but all other fields are included in the label.
  base::android::ScopedJavaLocalRef<jobjectArray> GetProfileLabels(
      JNIEnv* env,
      bool address_only,
      bool include_name_in_label,
      bool include_organization_in_label,
      bool include_country_in_label,
      std::vector<AutofillProfile*> profiles);

  // Returns the shipping label of the given profile for PaymentRequest. This
  // label does not contain the full name or the email address but will include
  // the country depending on the value of |include_country_in_label|. All other
  // fields are included in the label.
  base::android::ScopedJavaLocalRef<jstring>
  GetShippingAddressLabelForPaymentRequest(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jprofile,
      bool inlude_country_in_label);

  // Pointer to the java counterpart.
  JavaObjectWeakGlobalRef weak_java_obj_;

  // Pointer to the PersonalDataManager for the main profile.
  PersonalDataManager* personal_data_manager_;

  // Used for subkey request.
  SubKeyRequester subkey_requester_;

  DISALLOW_COPY_AND_ASSIGN(PersonalDataManagerAndroid);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ANDROID_PERSONAL_DATA_MANAGER_ANDROID_H_
