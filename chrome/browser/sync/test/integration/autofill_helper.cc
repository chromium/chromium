// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/autofill_helper.h"

#include <stddef.h>

#include <map>
#include <optional>
#include <ostream>
#include <sstream>
#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/uuid.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/webdata_services/web_data_service_factory.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_test_utils.h"
#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_entry.h"
#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/webdata/common/web_database.h"

using autofill::AutocompleteChangeList;
using autofill::AutocompleteEntry;
using autofill::AutocompleteKey;
using autofill::AutofillProfile;
using autofill::AutofillWebDataService;
using autofill::AutofillWebDataServiceObserverOnDBSequence;
using autofill::CreditCard;
using autofill::FormFieldData;
using autofill::PersonalDataManager;
using base::WaitableEvent;
using sync_datatype_helper::test;

namespace {

ACTION_P(SignalEvent, event) {
  event->Signal();
}

class MockWebDataServiceObserver
    : public AutofillWebDataServiceObserverOnDBSequence {
 public:
  MOCK_METHOD(void,
              AutocompleteEntriesChanged,
              (const AutocompleteChangeList& changes),
              (override));
};

scoped_refptr<AutofillWebDataService> GetWebDataService(int index) {
  return WebDataServiceFactory::GetAutofillWebDataForProfile(
      test()->GetProfile(index), ServiceAccessType::EXPLICIT_ACCESS);
}

void WaitForCurrentTasksToComplete(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  task_runner->PostTask(FROM_HERE, base::BindOnce(&base::WaitableEvent::Signal,
                                                  base::Unretained(&event)));
  event.Wait();
}

void RemoveKeyDontBlockForSync(int profile, const AutocompleteKey& key) {
  WaitableEvent done_event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);

  MockWebDataServiceObserver mock_observer;
  EXPECT_CALL(mock_observer, AutocompleteEntriesChanged)
      .WillOnce(SignalEvent(&done_event));

  scoped_refptr<AutofillWebDataService> wds = GetWebDataService(profile);

  void (AutofillWebDataService::*add_observer_func)(
      AutofillWebDataServiceObserverOnDBSequence*) =
      &AutofillWebDataService::AddObserver;
  wds->GetDBTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(add_observer_func, wds, &mock_observer));

  wds->RemoveFormValueForElementName(key.name(), key.value());
  done_event.Wait();

  void (AutofillWebDataService::*remove_observer_func)(
      AutofillWebDataServiceObserverOnDBSequence*) =
      &AutofillWebDataService::RemoveObserver;
  wds->GetDBTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(remove_observer_func, wds, &mock_observer));
}

void GetAllAutocompleteEntriesOnDBSequence(
    AutofillWebDataService* wds,
    std::vector<AutocompleteEntry>* entries) {
  DCHECK(wds->GetDBTaskRunner()->RunsTasksInCurrentSequence());
  autofill::AutocompleteTable::FromWebDatabase(wds->GetDatabase())
      ->GetAllAutocompleteEntries(entries);
}

std::vector<AutocompleteEntry> GetAllAutocompleteEntries(
    AutofillWebDataService* wds) {
  std::vector<AutocompleteEntry> entries;
  wds->GetDBTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&GetAllAutocompleteEntriesOnDBSequence,
                                base::Unretained(wds), &entries));
  WaitForCurrentTasksToComplete(wds->GetDBTaskRunner());
  return entries;
}

bool ProfilesMatchImpl(
    const std::optional<unsigned int>& expected_count,
    int profile_a,
    const std::vector<const AutofillProfile*>& autofill_profiles_a,
    int profile_b,
    const std::vector<const AutofillProfile*>& autofill_profiles_b,
    std::ostream* os) {
  if (expected_count.has_value() &&
      autofill_profiles_a.size() != *expected_count) {
    *os << "Profile " << profile_a
        << " does not have expected count of entities " << *expected_count;
    return false;
  }

  std::map<std::string, AutofillProfile> autofill_profiles_a_map;
  for (const AutofillProfile* p : autofill_profiles_a) {
    autofill_profiles_a_map.insert({p->guid(), *p});
  }

  // This seems to be a transient state that will eventually be rectified by
  // data type logic. We don't need to check b for duplicates directly because
  // after the first is erased from |autofill_profiles_a_map| the second will
  // not be found.
  if (autofill_profiles_a.size() != autofill_profiles_a_map.size()) {
    *os << "Profile " << profile_a << " contains duplicate GUID(s).";
    return false;
  }

  for (const AutofillProfile* p : autofill_profiles_b) {
    if (!autofill_profiles_a_map.count(p->guid())) {
      *os << "GUID " << p->guid() << " not found in profile " << profile_b
          << ".";
      return false;
    }

    auto profiles_a_it = autofill_profiles_a_map.find(p->guid());

    if (profiles_a_it == autofill_profiles_a_map.end()) {
      *os << "Profile with GUID " << p->guid() << " was not found.";
      return false;
    }

    AutofillProfile* expected_profile = &profiles_a_it->second;
    expected_profile->set_guid(p->guid());
    if (*expected_profile != *p) {
      *os << "Mismatch in profile with GUID " << p->guid() << ".";
      return false;
    }
    autofill_profiles_a_map.erase(p->guid());
  }

  if (!autofill_profiles_a_map.empty()) {
    *os << "Entries present in Profile " << profile_a << " but not in "
        << profile_b << ".";
    return false;
  }
  return true;
}

}  // namespace

namespace autofill_helper {

AutofillProfile CreateAutofillProfile(ProfileType type) {
  AutofillProfile profile(
      autofill::i18n_model_definition::kLegacyHierarchyCountryCode);
  switch (type) {
    case PROFILE_MARION:
      autofill::test::SetProfileInfoWithGuid(
          &profile, "C837507A-6C3B-4872-AC14-5113F157D668", "Marion",
          "Mitchell", "Morrison", "johnwayne@me.xyz", "Fox", "123 Zoo St.",
          "unit 5", "Hollywood", "CA", "91601", "US", "12345678910");
      break;
    case PROFILE_HOMER:
      autofill::test::SetProfileInfoWithGuid(
          &profile, "137DE1C3-6A30-4571-AC86-109B1ECFBE7F", "Homer", "J.",
          "Simpson", "homer@abc.com", "SNPP", "742 Evergreen Terrace",
          "PO Box 1", "Springfield", "MA", "94101", "US", "14155551212");
      break;
    case PROFILE_FRASIER:
      autofill::test::SetProfileInfoWithGuid(
          &profile, "9A5E6872-6198-4688-BF75-0016E781BB0A", "Frasier",
          "Winslow", "Crane", "", "randomness", "", "Apt. 4", "Seattle", "WA",
          "99121", "US", "0000000000");
      break;
    case PROFILE_NULL:
      autofill::test::SetProfileInfoWithGuid(
          &profile, "FE461507-7E13-4198-8E66-74C7DB6D8322", "", "", "", "", "",
          "", "", "", "", "", "", "");
      break;
  }
  profile.FinalizeAfterImport();
  return profile;
}

AutofillProfile CreateUniqueAutofillProfile() {
  AutofillProfile profile(AddressCountryCode("US"));
  autofill::test::SetProfileInfoWithGuid(
      &profile, base::Uuid::GenerateRandomV4().AsLowercaseString().c_str(),
      "First", "Middle", "Last", "email@domain.tld", "Company", "123 Main St",
      "Apt 456", "Nowhere", "OK", "73038", "US", "12345678910");
  profile.FinalizeAfterImport();
  return profile;
}

PersonalDataManager* GetPersonalDataManager(int index) {
  return autofill::PersonalDataManagerFactory::GetForBrowserContext(
      test()->GetProfile(index));
}

void AddKeys(int profile, const std::set<AutocompleteKey>& keys) {
  std::vector<FormFieldData> form_fields;
  for (const AutocompleteKey& key : keys) {
    FormFieldData field;
    field.set_name(key.name());
    field.set_value(key.value());
    form_fields.push_back(field);
  }

  WaitableEvent done_event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  MockWebDataServiceObserver mock_observer;
  EXPECT_CALL(mock_observer, AutocompleteEntriesChanged)
      .WillOnce(SignalEvent(&done_event));

  scoped_refptr<AutofillWebDataService> wds = GetWebDataService(profile);

  void (AutofillWebDataService::*add_observer_func)(
      AutofillWebDataServiceObserverOnDBSequence*) =
      &AutofillWebDataService::AddObserver;
  wds->GetDBTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(add_observer_func, wds, &mock_observer));

  wds->AddFormFields(form_fields);
  done_event.Wait();
  WaitForCurrentTasksToComplete(wds->GetDBTaskRunner());

  void (AutofillWebDataService::*remove_observer_func)(
      AutofillWebDataServiceObserverOnDBSequence*) =
      &AutofillWebDataService::RemoveObserver;
  wds->GetDBTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(remove_observer_func, wds, &mock_observer));
}

void RemoveKey(int profile, const AutocompleteKey& key) {
  RemoveKeyDontBlockForSync(profile, key);
  WaitForCurrentTasksToComplete(GetWebDataService(profile)->GetDBTaskRunner());
}

void RemoveKeys(int profile) {
  for (const AutocompleteKey& key : GetAllKeys(profile)) {
    RemoveKeyDontBlockForSync(profile, key);
  }
  WaitForCurrentTasksToComplete(GetWebDataService(profile)->GetDBTaskRunner());
}

std::set<AutocompleteKey> GetAllKeys(int profile) {
  scoped_refptr<AutofillWebDataService> wds = GetWebDataService(profile);
  std::set<AutocompleteKey> result;
  for (const AutocompleteEntry& entry : GetAllAutocompleteEntries(wds.get())) {
    result.insert(entry.key());
  }
  return result;
}

bool KeysMatch(int profile_a, int profile_b) {
  return GetAllKeys(profile_a) == GetAllKeys(profile_b);
}

void SetCreditCards(int profile, std::vector<CreditCard>* credit_cards) {
  GetPersonalDataManager(profile)->payments_data_manager().SetCreditCards(
      credit_cards);
}

void AddProfile(int profile, const AutofillProfile& autofill_profile) {
  PersonalDataManager* pdm = GetPersonalDataManager(profile);
  autofill::PersonalDataChangedWaiter waiter(*pdm);
  pdm->address_data_manager().AddProfile(autofill_profile);
  std::move(waiter).Wait();
}

void RemoveProfile(int profile, const std::string& guid) {
  PersonalDataManager* pdm = GetPersonalDataManager(profile);
  autofill::PersonalDataChangedWaiter waiter(*pdm);
  pdm->RemoveByGUID(guid);
  std::move(waiter).Wait();
}

void UpdateProfile(int profile,
                   const std::string& guid,
                   autofill::FieldType type,
                   const std::u16string& value,
                   autofill::VerificationStatus status) {
  PersonalDataManager* pdm = GetPersonalDataManager(profile);
  const AutofillProfile* pdm_profile =
      pdm->address_data_manager().GetProfileByGUID(guid);
  ASSERT_TRUE(pdm_profile);
  AutofillProfile updated_profile = *pdm_profile;
  updated_profile.SetRawInfoWithVerificationStatus(type, value, status);
  autofill::PersonalDataChangedWaiter waiter(*pdm);
  pdm->address_data_manager().UpdateProfile(updated_profile);
  std::move(waiter).Wait();
}

std::vector<const AutofillProfile*> GetAllAutoFillProfiles(int profile) {
  PersonalDataManager* pdm = GetPersonalDataManager(profile);
  autofill::PersonalDataChangedWaiter waiter(*pdm);
  pdm->Refresh();
  // PersonalDataManager::GetProfiles() simply returns the current values that
  // have been last reported to the UI sequence. PersonalDataManager::Refresh()
  // will post a task to the DB sequence to read back the latest values, and we
  // very much want the latest values. Unfortunately, the Refresh() call is
  // asynchronous and there's no way to pass a callback that's run when our
  // Refresh() call finishes. A PersonalDataManagerObserver won't completely fix
  // the problem either since there could be multiple outstanding modifications
  // scheduled, and we cannot ensure that we have the latest view. Instead
  // explicitly wait for our Refresh to have executed. It is possible for
  // another write to sneak in between our Refresh() and the task that is
  // blocked for, causing the GetProfiles() read to return even more current
  // data, but this shouldn't cause problems. While PersonalDataManager will
  // cancel outstanding queries, this is only instigated on the UI sequence,
  // which we are about to block, which means we are safe.
  WaitForCurrentTasksToComplete(GetWebDataService(profile)->GetDBTaskRunner());
  std::move(waiter).Wait();
  return pdm->address_data_manager().GetProfiles();
}

size_t GetProfileCount(int profile) {
  return GetAllAutoFillProfiles(profile).size();
}

size_t GetKeyCount(int profile) {
  return GetAllKeys(profile).size();
}

bool ProfilesMatch(int profile_a, int profile_b) {
  const std::vector<const AutofillProfile*>& autofill_profiles_a =
      GetAllAutoFillProfiles(profile_a);
  const std::vector<const AutofillProfile*>& autofill_profiles_b =
      GetAllAutoFillProfiles(profile_b);
  std::ostringstream mismatch_reason_stream;
  bool matched =
      ProfilesMatchImpl(std::nullopt, profile_a, autofill_profiles_a, profile_b,
                        autofill_profiles_b, &mismatch_reason_stream);
  if (!matched) {
    DLOG(INFO) << "Profiles mismatch: " << mismatch_reason_stream.str();
  }
  return matched;
}

}  // namespace autofill_helper

AutocompleteKeysChecker::AutocompleteKeysChecker(int profile_a, int profile_b)
    : MultiClientStatusChangeChecker(
          sync_datatype_helper::test()->GetSyncServices()),
      profile_a_(profile_a),
      profile_b_(profile_b) {}

bool AutocompleteKeysChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for matching autofill keys";
  return autofill_helper::KeysMatch(profile_a_, profile_b_);
}

AutofillProfileChecker::AutofillProfileChecker(
    int profile_a,
    int profile_b,
    std::optional<unsigned int> expected_count)
    : profile_a_(profile_a),
      profile_b_(profile_b),
      expected_count_(expected_count) {
  autofill_helper::GetPersonalDataManager(profile_a_)
      ->address_data_manager()
      .AddObserver(this);
  autofill_helper::GetPersonalDataManager(profile_b_)
      ->address_data_manager()
      .AddObserver(this);
}

AutofillProfileChecker::~AutofillProfileChecker() {
  autofill_helper::GetPersonalDataManager(profile_a_)
      ->address_data_manager()
      .RemoveObserver(this);
  autofill_helper::GetPersonalDataManager(profile_b_)
      ->address_data_manager()
      .RemoveObserver(this);
}
bool AutofillProfileChecker::Wait() {
  DLOG(WARNING) << "AutofillProfileChecker::Wait() started";
  PersonalDataManager* pdm_a =
      autofill_helper::GetPersonalDataManager(profile_a_);
  PersonalDataManager* pdm_b =
      autofill_helper::GetPersonalDataManager(profile_b_);

  autofill::PersonalDataChangedWaiter waiter_a(*pdm_a);
  pdm_a->Refresh();
  // Similar to GetAllAutoFillProfiles() we need to make sure we are not reading
  // before any locally instigated async writes. This is run exactly one time
  // before the first IsExitConditionSatisfied() is called.
  WaitForCurrentTasksToComplete(
      GetWebDataService(profile_a_)->GetDBTaskRunner());
  std::move(waiter_a).Wait();

  autofill::PersonalDataChangedWaiter waiter_b(*pdm_b);
  pdm_b->Refresh();
  WaitForCurrentTasksToComplete(
      GetWebDataService(profile_b_)->GetDBTaskRunner());
  std::move(waiter_b).Wait();

  DLOG(WARNING) << "AutofillProfileChecker::Wait() completed";
  return StatusChangeChecker::Wait();
}

bool AutofillProfileChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for matching autofill profiles";
  const std::vector<const AutofillProfile*>& autofill_profiles_a =
      autofill_helper::GetPersonalDataManager(profile_a_)
          ->address_data_manager()
          .GetProfiles();
  const std::vector<const AutofillProfile*>& autofill_profiles_b =
      autofill_helper::GetPersonalDataManager(profile_b_)
          ->address_data_manager()
          .GetProfiles();
  return ProfilesMatchImpl(expected_count_, profile_a_, autofill_profiles_a,
                           profile_b_, autofill_profiles_b, os);
}

void AutofillProfileChecker::OnAddressDataChanged() {
  CheckExitCondition();
}
