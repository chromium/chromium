// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ANDROID_PERSONAL_DATA_MANAGER_ANDROID_H_
#define CHROME_BROWSER_AUTOFILL_ANDROID_PERSONAL_DATA_MANAGER_ANDROID_H_

#include <vector>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"

class PrefService;

namespace autofill {

// Android wrapper of the PersonalDataManager which provides access from the
// Java layer. Note that on Android, there's only a single profile, and
// therefore a single instance of this wrapper.
class PersonalDataManagerAndroid : public PersonalDataManagerObserver {
 public:
  PersonalDataManagerAndroid(JNIEnv* env,
                             const jni_zero::JavaRef<jobject>& obj,
                             PersonalDataManager* personal_data_manager,
                             PrefService* prefs);

  PersonalDataManagerAndroid(const PersonalDataManagerAndroid&) = delete;
  PersonalDataManagerAndroid& operator=(const PersonalDataManagerAndroid&) =
      delete;

  // Trigger the destruction of the C++ object from Java.
  void Destroy(JNIEnv* env);

  static base::android::ScopedJavaLocalRef<jobject>
  CreateJavaCreditCardFromNative(JNIEnv* env, const CreditCard& card);
  static void PopulateNativeCreditCardFromJava(
      const base::android::JavaRef<jobject>& jcard,
      JNIEnv* env,
      CreditCard* card);

  // Returns true if personal data manager has loaded the initial data.
  jboolean IsDataLoaded(JNIEnv* env) const;

  // These functions act on "web profiles" aka "LOCAL_PROFILE" profiles.
  // -------------------------

  // Returns the GUIDs of all profiles.
  base::android::ScopedJavaLocalRef<jobjectArray> GetProfileGUIDsForSettings(
      JNIEnv* env);

  // Returns the GUIDs of the profiles to suggest to the user. See
  // PersonalDataManager::GetProfilesToSuggest for more details.
  base::android::ScopedJavaLocalRef<jobjectArray> GetProfileGUIDsToSuggest(
      JNIEnv* env);

  // Returns the profile with the specified |jguid|, or NULL if there is no
  // profile with the specified |jguid|. Both web and auxiliary profiles may
  // be returned.
  base::android::ScopedJavaLocalRef<jobject> GetProfileByGUID(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& jguid);

  // Determines whether the logged in user (if any) is eligible to store
  // Autofill address profiles to their account.
  jboolean IsEligibleForAddressAccountStorage(JNIEnv* env);

  // Determines the country for for the newly created address profile.
  base::android::ScopedJavaLocalRef<jstring> GetDefaultCountryCodeForNewAddress(
      JNIEnv* env) const;

  // Users based in unsupported countries and profiles with a country value set
  // to an unsupported country are not eligible for account storage. This
  // function determines if the `country_code` is eligible.
  bool IsCountryEligibleForAccountStorage(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& country_code) const;

  // Adds or modifies a profile.  If |jguid| is an empty string, we are creating
  // a new profile.  Else we are updating an existing profile.  Always returns
  // the GUID for this profile; the GUID it may have just been created.
  base::android::ScopedJavaLocalRef<jstring> SetProfile(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jprofile,
      const base::android::JavaParamRef<jstring>& jguid);
  // Adds or modifies a profile like SetProfile interface if |jprofile| is
  // local. Otherwise it creates a local copy of it.
  base::android::ScopedJavaLocalRef<jstring> SetProfileToLocal(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jprofile,
      const base::android::JavaParamRef<jstring>& jguid);

  // Gets the labels for all known profiles. These labels are useful for
  // distinguishing the profiles from one another.
  //
  // The labels never contain the full name and include at least 2 fields.
  base::android::ScopedJavaLocalRef<jobjectArray> GetProfileLabelsForSettings(
      JNIEnv* env);

  // Gets the labels for the profiles to suggest to the user. These labels are
  // useful for distinguishing the profiles from one another.
  //
  // The labels never contain the email address, or phone numbers. The
  // |include_name_in_label| argument controls whether the name is included.
  // All other fields are included in the label.
  base::android::ScopedJavaLocalRef<jobjectArray> GetProfileLabelsToSuggest(
      JNIEnv* env,
      jboolean include_name_in_label,
      jboolean include_organization_in_label,
      jboolean include_country_in_label);

  // Returns the shipping label of the given profile for PaymentRequest. This
  // label does not contain the full name or the email address but will include
  // the country depending on the value of |include_country_in_label|. All other
  // fields are included in the label.
  base::android::ScopedJavaLocalRef<jstring>
  GetShippingAddressLabelForPaymentRequest(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jprofile,
      const base::android::JavaParamRef<jstring>& jguid,
      bool include_country_in_label);

  // These functions act on local credit cards.
  // --------------------

  // Returns the GUIDs of all the credit cards.
  base::android::ScopedJavaLocalRef<jobjectArray> GetCreditCardGUIDsForSettings(
      JNIEnv* env);

  // Returns the GUIDs of the credit cards to suggest to the user. See
  // PersonalDataManager::GetCreditCardsToSuggest for more details.
  base::android::ScopedJavaLocalRef<jobjectArray> GetCreditCardGUIDsToSuggest(
      JNIEnv* env);

  // Returns the credit card with the specified |jguid|, or NULL if there is
  // no credit card with the specified |jguid|.
  base::android::ScopedJavaLocalRef<jobject> GetCreditCardByGUID(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& jguid);

  // Returns a credit card with the specified |jcard_number|. This is used for
  // determining the card's obfuscated number, issuer icon, and type in one go.
  // This function does not interact with the autofill table on disk, so can be
  // used for cards that are not saved.
  base::android::ScopedJavaLocalRef<jobject> GetCreditCardForNumber(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& jcard_number);

  // Adds or modifies a local credit card.  If |jguid| is an empty string, we
  // are creating a new card.  Else we are updating an existing profile.  Always
  // returns the GUID for this profile; the GUID it may have just been created.
  base::android::ScopedJavaLocalRef<jstring> SetCreditCard(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcard);

  // Updates the billing address of a server credit card |jcard|.
  void UpdateServerCardBillingAddress(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcard);

  // Removes the profile or credit card represented by |jguid|.
  void RemoveByGUID(JNIEnv* env,
                    const base::android::JavaParamRef<jstring>& jguid);

  // Delete all local credit cards.
  void DeleteAllLocalCreditCards(JNIEnv* env);

  // PersonalDataManagerObserver:
  void OnPersonalDataChanged() override;

  // These functions act on the usage stats of local profiles and credit cards.
  // --------------------

  // Records the use and log usage metrics for the profile associated with the
  // |jguid|. Increments the use count of the profile and sets its use date to
  // the current time.
  void RecordAndLogProfileUse(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& jguid);

  // Records the use and log usage metrics for the credit card associated with
  // the |jguid|. Increments the use count of the credit card and sets its use
  // date to the current time.
  void RecordAndLogCreditCardUse(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& jguid);

  // Checks whether the Autofill PersonalDataManager has profiles.
  jboolean HasProfiles(JNIEnv* env);

  // Checks whether the Autofill PersonalDataManager has credit cards.
  jboolean HasCreditCards(JNIEnv* env);

  // Checks whether FIDO authentication is available.
  jboolean IsFidoAuthenticationAvailable(JNIEnv* env);

  // Get Java AutofillImageFetcher.
  base::android::ScopedJavaLocalRef<jobject> GetOrCreateJavaImageFetcher(
      JNIEnv* env);

  static base::android::ScopedJavaLocalRef<jobject> CreateJavaIbanFromNative(
      JNIEnv* env,
      const Iban& iban);

  static void PopulateNativeIbanFromJava(
      const base::android::JavaRef<jobject>& jiban,
      JNIEnv* env,
      Iban* iban);

  // Add a server IBAN. Used only in tests.
  void AddServerIbanForTest(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& jiban);

  // Return IBAN with the specified `jguid`, or Null if there is no IBAN with
  // the specified `jguid`.
  base::android::ScopedJavaLocalRef<jobject> GetIbanByGuid(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& jguid);

  // Returns an array of all stored IBANs.
  base::android::ScopedJavaLocalRef<jobjectArray> GetIbansForSettings(
      JNIEnv* env);

  // Adds or modifies a local IBAN. If `jiban`'s GUID is an empty string we
  // create a new IBAN, otherwise we update the existing IBAN. Always returns
  // the GUID for this IBAN; the GUID may have just been created.
  base::android::ScopedJavaLocalRef<jstring> AddOrUpdateLocalIban(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jiban);

  // Checks if `jiban_value` is a valid IBAN.
  static jboolean IsValidIban(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& jiban_value);

  // Returns whether the `Add IBAN` button should be shown on the payment
  // methods settings page.
  jboolean ShouldShowAddIbanButtonOnSettingsPage(JNIEnv* env);

  // Returns whether the Autofill feature is managed.
  jboolean IsAutofillManaged(JNIEnv* env);

  // Returns whether the Autofill feature for profiles is managed.
  jboolean IsAutofillProfileManaged(JNIEnv* env);

  // Returns whether the Autofill feature for credit cards is managed.
  jboolean IsAutofillCreditCardManaged(JNIEnv* env);

  // Returns an array of BankAccount objects retrieved from the
  // PersonalDataManager.
  base::android::ScopedJavaLocalRef<jobjectArray> GetMaskedBankAccounts(
      JNIEnv* env);

  // Create an object of Java BankAccount from native BankAccount.
  static base::android::ScopedJavaLocalRef<jobject>
  CreateJavaBankAccountFromNative(JNIEnv* env, const BankAccount& bank_account);

  // Create an object of native BankAccount from Java BankAccount.
  static BankAccount CreateNativeBankAccountFromJava(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jbank_account);

 private:
  ~PersonalDataManagerAndroid() override;

  // Returns the GUIDs of the |profiles| passed as parameter.
  base::android::ScopedJavaLocalRef<jobjectArray> GetProfileGUIDs(
      JNIEnv* env,
      const std::vector<const AutofillProfile*>& profiles);

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
      std::vector<const AutofillProfile*> profiles);

  // Pointer to the java counterpart.
  JavaObjectWeakGlobalRef weak_java_obj_;

  // Pointer to the PersonalDataManager for the main profile.
  raw_ptr<PersonalDataManager> personal_data_manager_;

  raw_ptr<PrefService> prefs_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ANDROID_PERSONAL_DATA_MANAGER_ANDROID_H_
