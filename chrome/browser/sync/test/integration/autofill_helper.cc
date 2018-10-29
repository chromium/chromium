// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/autofill_helper.h"

#include <stddef.h>

#include <map>
#include <utility>

#include "base/guid.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/web_data_service_factory.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/webdata/autofill_entry.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/webdata/common/web_database.h"

using autofill::AutofillChangeList;
using autofill::AutofillEntry;
using autofill::AutofillKey;
using autofill::AutofillProfile;
using autofill::AutofillTable;
using autofill::AutofillType;
using autofill::AutofillWebDataService;
using autofill::AutofillWebDataServiceObserverOnDBSequence;
using autofill::CreditCard;
using autofill::FormFieldData;
using autofill::PersonalDataManager;
using autofill::PersonalDataManagerObserver;
using base::WaitableEvent;
using sync_datatype_helper::test;
using testing::_;

namespace {

ACTION_P(SignalEvent, event) {
  event->Signal();
}

class MockWebDataServiceObserver
    : public AutofillWebDataServiceObserverOnDBSequence {
 public:
  MOCK_METHOD1(AutofillEntriesChanged,
               void(const AutofillChangeList& changes));
};

void WaitForCurrentTasksToComplete(base::SequencedTaskRunner* task_runner) {
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  task_runner->PostTask(FROM_HERE, base::BindOnce(&base::WaitableEvent::Signal,
                                                  base::Unretained(&event)));
  event.Wait();
}

void RemoveKeyDontBlockForSync(int profile, const AutofillKey& key) {
  WaitableEvent done_event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);

  MockWebDataServiceObserver mock_observer;
  EXPECT_CALL(mock_observer, AutofillEntriesChanged(_))
      .WillOnce(SignalEvent(&done_event));

  scoped_refptr<AutofillWebDataService> wds =
      autofill_helper::GetProfileWebDataService(profile);

  void (AutofillWebDataService::*add_observer_func)(
      AutofillWebDataServiceObserverOnDBSequence*) =
      &AutofillWebDataService::AddObserver;
  wds->GetDBTaskRunner()->PostTask(
      FROM_HERE, base::Bind(add_observer_func, wds, &mock_observer));

  wds->RemoveFormValueForElementName(key.name(), key.value());
  done_event.Wait();

  void (AutofillWebDataService::*remove_observer_func)(
      AutofillWebDataServiceObserverOnDBSequence*) =
      &AutofillWebDataService::RemoveObserver;
  wds->GetDBTaskRunner()->PostTask(
      FROM_HERE, base::Bind(remove_observer_func, wds, &mock_observer));
}

void GetAllAutofillEntriesOnDBSequence(AutofillWebDataService* wds,
                                       std::vector<AutofillEntry>* entries) {
  DCHECK(wds->GetDBTaskRunner()->RunsTasksInCurrentSequence());
  AutofillTable::FromWebDatabase(
      wds->GetDatabase())->GetAllAutofillEntries(entries);
}

std::vector<AutofillEntry> GetAllAutofillEntries(AutofillWebDataService* wds) {
  std::vector<AutofillEntry> entries;
  wds->GetDBTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&GetAllAutofillEntriesOnDBSequence,
                            base::Unretained(wds), &entries));
  WaitForCurrentTasksToComplete(wds->GetDBTaskRunner());
  return entries;
}

void SetServerCardsOnDBSequence(
    AutofillWebDataService* wds,
    const std::vector<autofill::CreditCard>& credit_cards) {
  DCHECK(wds->GetDBTaskRunner()->RunsTasksInCurrentSequence());
  AutofillTable::FromWebDatabase(wds->GetDatabase())
      ->SetServerCreditCards(credit_cards);
}

void SetServerProfilesOnDBSequence(
    AutofillWebDataService* wds,
    const std::vector<autofill::AutofillProfile>& profiles) {
  DCHECK(wds->GetDBTaskRunner()->RunsTasksInCurrentSequence());
  AutofillTable::FromWebDatabase(wds->GetDatabase())
      ->SetServerProfiles(profiles);
}

void SetPaymentsCustomerDataOnDBSequence(
    AutofillWebDataService* wds,
    const autofill::PaymentsCustomerData& customer_data) {
  DCHECK(wds->GetDBTaskRunner()->RunsTasksInCurrentSequence());
  AutofillTable::FromWebDatabase(wds->GetDatabase())
      ->SetPaymentsCustomerData(&customer_data);
}

bool ProfilesMatchImpl(
    int profile_a,
    const std::vector<AutofillProfile*>& autofill_profiles_a,
    int profile_b,
    const std::vector<AutofillProfile*>& autofill_profiles_b) {
  std::map<std::string, AutofillProfile> autofill_profiles_a_map;
  for (AutofillProfile* p : autofill_profiles_a) {
    autofill_profiles_a_map[p->guid()] = *p;
  }

  // This seems to be a transient state that will eventually be rectified by
  // model type logic. We don't need to check b for duplicates directly because
  // after the first is erased from |autofill_profiles_a_map| the second will
  // not be found.
  if (autofill_profiles_a.size() != autofill_profiles_a_map.size()) {
    DVLOG(1) << "Profile " << profile_a << " contains duplicate GUID(s).";
    return false;
  }

  for (AutofillProfile* p : autofill_profiles_b) {
    if (!autofill_profiles_a_map.count(p->guid())) {
      DVLOG(1) << "GUID " << p->guid() << " not found in profile " << profile_b
               << ".";
      return false;
    }
    AutofillProfile* expected_profile = &autofill_profiles_a_map[p->guid()];
    expected_profile->set_guid(p->guid());
    if (*expected_profile != *p) {
      DVLOG(1) << "Mismatch in profile with GUID " << p->guid() << ".";
      return false;
    }
    autofill_profiles_a_map.erase(p->guid());
  }

  if (autofill_profiles_a_map.size()) {
    DVLOG(1) << "Entries present in Profile " << profile_a << " but not in "
             << profile_b << ".";
    return false;
  }
  return true;
}

}  // namespace

namespace autofill_helper {

AutofillProfile CreateAutofillProfile(ProfileType type) {
  AutofillProfile profile;
  switch (type) {
    case PROFILE_MARION:
      autofill::test::SetProfileInfoWithGuid(&profile,
          "C837507A-6C3B-4872-AC14-5113F157D668",
          "Marion", "Mitchell", "Morrison",
          "johnwayne@me.xyz", "Fox",
          "123 Zoo St.", "unit 5", "Hollywood", "CA",
          "91601", "US", "12345678910");
      break;
    case PROFILE_HOMER:
      autofill::test::SetProfileInfoWithGuid(&profile,
          "137DE1C3-6A30-4571-AC86-109B1ECFBE7F",
          "Homer", "J.", "Simpson",
          "homer@abc.com", "SNPP",
          "742 Evergreen Terrace", "PO Box 1", "Springfield", "MA",
          "94101", "US", "14155551212");
      break;
    case PROFILE_FRASIER:
      autofill::test::SetProfileInfoWithGuid(&profile,
          "9A5E6872-6198-4688-BF75-0016E781BB0A",
          "Frasier", "Winslow", "Crane",
          "", "randomness", "", "Apt. 4", "Seattle", "WA",
          "99121", "US", "0000000000");
      break;
    case PROFILE_NULL:
      autofill::test::SetProfileInfoWithGuid(&profile,
          "FE461507-7E13-4198-8E66-74C7DB6D8322",
          "", "", "", "", "", "", "", "", "", "", "", "");
      break;
  }
  return profile;
}

AutofillProfile CreateUniqueAutofillProfile() {
  AutofillProfile profile;
  autofill::test::SetProfileInfoWithGuid(&profile,
      base::GenerateGUID().c_str(),
      "First", "Middle", "Last",
      "email@domain.tld", "Company",
      "123 Main St", "Apt 456", "Nowhere", "OK",
      "73038", "US", "12345678910");
  return profile;
}

scoped_refptr<AutofillWebDataService> GetProfileWebDataService(int index) {
  return WebDataServiceFactory::GetAutofillWebDataForProfile(
      test()->GetProfile(index), ServiceAccessType::EXPLICIT_ACCESS);
}

scoped_refptr<AutofillWebDataService> GetAccountWebDataService(int index) {
  return WebDataServiceFactory::GetAutofillWebDataForAccount(
      test()->GetProfile(index), ServiceAccessType::EXPLICIT_ACCESS);
}

PersonalDataManager* GetPersonalDataManager(int index) {
  return autofill::PersonalDataManagerFactory::GetForProfile(
      test()->GetProfile(index));
}

void AddKeys(int profile, const std::set<AutofillKey>& keys) {
  std::vector<FormFieldData> form_fields;
  for (const AutofillKey& key : keys) {
    FormFieldData field;
    field.name = key.name();
    field.value = key.value();
    form_fields.push_back(field);
  }

  WaitableEvent done_event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  MockWebDataServiceObserver mock_observer;
  EXPECT_CALL(mock_observer, AutofillEntriesChanged(_))
      .WillOnce(SignalEvent(&done_event));

  scoped_refptr<AutofillWebDataService> wds = GetProfileWebDataService(profile);

  void (AutofillWebDataService::*add_observer_func)(
      AutofillWebDataServiceObserverOnDBSequence*) =
      &AutofillWebDataService::AddObserver;
  wds->GetDBTaskRunner()->PostTask(
      FROM_HERE, base::Bind(add_observer_func, wds, &mock_observer));

  wds->AddFormFields(form_fields);
  done_event.Wait();
  WaitForCurrentTasksToComplete(wds->GetDBTaskRunner());

  void (AutofillWebDataService::*remove_observer_func)(
      AutofillWebDataServiceObserverOnDBSequence*) =
      &AutofillWebDataService::RemoveObserver;
  wds->GetDBTaskRunner()->PostTask(
      FROM_HERE, base::Bind(remove_observer_func, wds, &mock_observer));
}

void RemoveKey(int profile, const AutofillKey& key) {
  RemoveKeyDontBlockForSync(profile, key);
  WaitForCurrentTasksToComplete(
      autofill_helper::GetProfileWebDataService(profile)->GetDBTaskRunner());
}

void RemoveKeys(int profile) {
  std::set<AutofillEntry> keys = GetAllKeys(profile);
  for (const AutofillEntry& entry : keys) {
    RemoveKeyDontBlockForSync(profile, entry.key());
  }
  WaitForCurrentTasksToComplete(
      autofill_helper::GetProfileWebDataService(profile)->GetDBTaskRunner());
}

std::set<AutofillEntry> GetAllKeys(int profile) {
  scoped_refptr<AutofillWebDataService> wds = GetProfileWebDataService(profile);
  std::vector<AutofillEntry> all_entries = GetAllAutofillEntries(wds.get());
  return std::set<AutofillEntry>(all_entries.begin(), all_entries.end());
}

bool KeysMatch(int profile_a, int profile_b) {
  return GetAllKeys(profile_a) == GetAllKeys(profile_b);
}

void SetProfiles(int profile, std::vector<AutofillProfile>* autofill_profiles) {
  GetPersonalDataManager(profile)->SetProfiles(autofill_profiles);
}

void SetCreditCards(int profile, std::vector<CreditCard>* credit_cards) {
  GetPersonalDataManager(profile)->SetCreditCards(credit_cards);
}

void SetServerCreditCards(
    int profile,
    const std::vector<autofill::CreditCard>& credit_cards) {
  scoped_refptr<AutofillWebDataService> wds = GetProfileWebDataService(profile);
  wds->GetDBTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&SetServerCardsOnDBSequence,
                                base::Unretained(wds.get()), credit_cards));
  WaitForCurrentTasksToComplete(wds->GetDBTaskRunner());
}

void SetServerProfiles(int profile,
                       const std::vector<autofill::AutofillProfile>& profiles) {
  scoped_refptr<AutofillWebDataService> wds = GetProfileWebDataService(profile);
  wds->GetDBTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&SetServerProfilesOnDBSequence,
                                base::Unretained(wds.get()), profiles));
  WaitForCurrentTasksToComplete(wds->GetDBTaskRunner());
}

void SetPaymentsCustomerData(
    int profile,
    const autofill::PaymentsCustomerData& customer_data) {
  scoped_refptr<AutofillWebDataService> wds = GetProfileWebDataService(profile);
  wds->GetDBTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&SetPaymentsCustomerDataOnDBSequence,
                                base::Unretained(wds.get()), customer_data));
  WaitForCurrentTasksToComplete(wds->GetDBTaskRunner());
}

void AddProfile(int profile, const AutofillProfile& autofill_profile) {
  std::vector<AutofillProfile> autofill_profiles;
  for (AutofillProfile* profile : GetAllAutoFillProfiles(profile)) {
    autofill_profiles.push_back(*profile);
  }
  autofill_profiles.push_back(autofill_profile);
  autofill_helper::SetProfiles(profile, &autofill_profiles);
}

void RemoveProfile(int profile, const std::string& guid) {
  std::vector<AutofillProfile> autofill_profiles;
  for (AutofillProfile* profile : GetAllAutoFillProfiles(profile)) {
    if (profile->guid() != guid) {
      autofill_profiles.push_back(*profile);
    }
  }
  autofill_helper::SetProfiles(profile, &autofill_profiles);
}

void UpdateProfile(int profile,
                   const std::string& guid,
                   const AutofillType& type,
                   const base::string16& value) {
  std::vector<AutofillProfile> profiles;
  for (AutofillProfile* profile : GetAllAutoFillProfiles(profile)) {
    profiles.push_back(*profile);
    if (profile->guid() == guid) {
      profiles.back().SetRawInfo(type.GetStorableType(), value);
    }
  }
  autofill_helper::SetProfiles(profile, &profiles);
}

std::vector<AutofillProfile*> GetAllAutoFillProfiles(int profile) {
  PersonalDataManager* pdm = GetPersonalDataManager(profile);
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
  WaitForCurrentTasksToComplete(
      autofill_helper::GetProfileWebDataService(profile)->GetDBTaskRunner());

  return pdm->GetProfiles();
}

size_t GetProfileCount(int profile) {
  return GetAllAutoFillProfiles(profile).size();
}

size_t GetKeyCount(int profile) {
  return GetAllKeys(profile).size();
}

bool ProfilesMatch(int profile_a, int profile_b) {
  const std::vector<AutofillProfile*>& autofill_profiles_a =
      GetAllAutoFillProfiles(profile_a);
  const std::vector<AutofillProfile*>& autofill_profiles_b =
      GetAllAutoFillProfiles(profile_b);
  return ProfilesMatchImpl(
      profile_a, autofill_profiles_a, profile_b, autofill_profiles_b);
}

}  // namespace autofill_helper

AutofillKeysChecker::AutofillKeysChecker(int profile_a, int profile_b)
    : MultiClientStatusChangeChecker(
          sync_datatype_helper::test()->GetSyncServices()),
      profile_a_(profile_a),
      profile_b_(profile_b) {}

bool AutofillKeysChecker::IsExitConditionSatisfied() {
  return autofill_helper::KeysMatch(profile_a_, profile_b_);
}

std::string AutofillKeysChecker::GetDebugMessage() const {
  return "Waiting for matching autofill keys";
}

AutofillProfileChecker::AutofillProfileChecker(int profile_a, int profile_b)
    : profile_a_(profile_a), profile_b_(profile_b) {
  autofill_helper::GetPersonalDataManager(profile_a_)->AddObserver(this);
  autofill_helper::GetPersonalDataManager(profile_b_)->AddObserver(this);
}

AutofillProfileChecker::~AutofillProfileChecker() {
  autofill_helper::GetPersonalDataManager(profile_a_)->RemoveObserver(this);
  autofill_helper::GetPersonalDataManager(profile_b_)->RemoveObserver(this);
}

bool AutofillProfileChecker::Wait() {
  autofill_helper::GetPersonalDataManager(profile_a_)->Refresh();
  autofill_helper::GetPersonalDataManager(profile_b_)->Refresh();
  // Similar to GetAllAutoFillProfiles() we need to make sure we are not reading
  // before any locally instigated async writes. This is run exactly one time
  // before the first IsExitConditionSatisfied() is called.
  WaitForCurrentTasksToComplete(
      autofill_helper::GetProfileWebDataService(profile_a_)->GetDBTaskRunner());
  WaitForCurrentTasksToComplete(
      autofill_helper::GetProfileWebDataService(profile_b_)->GetDBTaskRunner());
  return StatusChangeChecker::Wait();
}

bool AutofillProfileChecker::IsExitConditionSatisfied() {
  const std::vector<AutofillProfile*>& autofill_profiles_a =
      autofill_helper::GetPersonalDataManager(profile_a_)->GetProfiles();
  const std::vector<AutofillProfile*>& autofill_profiles_b =
      autofill_helper::GetPersonalDataManager(profile_b_)->GetProfiles();
  return ProfilesMatchImpl(profile_a_, autofill_profiles_a, profile_b_,
                           autofill_profiles_b);
}

std::string AutofillProfileChecker::GetDebugMessage() const {
  return "Waiting for matching autofill profiles";
}

void AutofillProfileChecker::OnPersonalDataChanged() {
  CheckExitCondition();
}
