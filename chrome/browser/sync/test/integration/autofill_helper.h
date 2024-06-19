// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_AUTOFILL_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_AUTOFILL_HELPER_H_

#include <set>
#include <string>
#include <vector>

#include "chrome/browser/sync/test/integration/multi_client_status_change_checker.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_component.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {
class AutocompleteKey;
class AutofillProfile;
class CreditCard;
class PersonalDataManager;

}  // namespace autofill

namespace autofill_helper {

enum ProfileType {
  PROFILE_MARION,
  PROFILE_HOMER,
  PROFILE_FRASIER,
  PROFILE_NULL
};

// Used to access the personal data manager within a particular sync profile.
[[nodiscard]] autofill::PersonalDataManager* GetPersonalDataManager(int index);

// Adds the form fields in |keys| to the WebDataService of sync profile
// |profile|.
void AddKeys(int profile, const std::set<autofill::AutocompleteKey>& keys);

// Removes the form field in |key| from the WebDataService of sync profile
// |profile|.
void RemoveKey(int profile, const autofill::AutocompleteKey& key);

// Removes all of the keys from the WebDataService of sync profile |profile|.
void RemoveKeys(int profile);

// Gets all the form fields in the WebDataService of sync profile |profile|.
[[nodiscard]] std::set<autofill::AutocompleteKey> GetAllKeys(int profile);

// Compares the form fields in the WebDataServices of sync profiles
// |profile_a| and |profile_b|. Returns true if they match.
[[nodiscard]] bool KeysMatch(int profile_a, int profile_b);

// Replaces the CreditCard profiles in sync profile |profile| with
// |credit_cards|.
void SetCreditCards(int profile,
                    std::vector<autofill::CreditCard>* credit_cards);

// Adds the autofill profile |autofill_profile| to sync profile |profile|.
void AddProfile(int profile, const autofill::AutofillProfile& autofill_profile);

// Removes the autofill profile with guid |guid| from sync profile
// |profile|.
void RemoveProfile(int profile, const std::string& guid);

// Updates the autofill profile with guid |guid| in sync profile |profile|
// to |type| and |value| with the verification status |status|.
void UpdateProfile(int profile,
                   const std::string& guid,
                   autofill::FieldType type,
                   const std::u16string& value,
                   autofill::VerificationStatus status =
                       autofill::VerificationStatus::kObserved);

// Gets all the Autofill profiles in the PersonalDataManager of sync profile
// |profile|.
[[nodiscard]] std::vector<const autofill::AutofillProfile*>
GetAllAutoFillProfiles(int profile);

// Returns the number of autofill profiles contained by sync profile
// |profile|.
size_t GetProfileCount(int profile);

// Returns the number of autofill keys contained by sync profile |profile|.
size_t GetKeyCount(int profile);

// Compares the Autofill profiles in the PersonalDataManagers of sync profiles
// |profile_a| and |profile_b|. Returns true if they match.
[[nodiscard]] bool ProfilesMatch(int profile_a, int profile_b);

// Creates a test autofill profile based on the persona specified in |type|.
autofill::AutofillProfile CreateAutofillProfile(ProfileType type);

// Creates a test autofill profile with a unique GUID
autofill::AutofillProfile CreateUniqueAutofillProfile();

}  // namespace autofill_helper

// Checker to block until autocomplete keys match on both profiles.
class AutocompleteKeysChecker : public MultiClientStatusChangeChecker {
 public:
  AutocompleteKeysChecker(int profile_a, int profile_b);

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  const int profile_a_;
  const int profile_b_;
};

// Checker to block until autofill profiles match on both profiles.
class AutofillProfileChecker : public StatusChangeChecker,
                               public autofill::AddressDataManager::Observer {
 public:
  AutofillProfileChecker(int profile_a,
                         int profile_b,
                         std::optional<unsigned int> expected_count);
  ~AutofillProfileChecker() override;

  // StatusChangeChecker implementation.
  bool Wait() override;
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // autofill::AddressDataManager::Observer implementation.
  void OnAddressDataChanged() override;

 private:
  const int profile_a_;
  const int profile_b_;
  const std::optional<unsigned int> expected_count_;
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_AUTOFILL_HELPER_H_
