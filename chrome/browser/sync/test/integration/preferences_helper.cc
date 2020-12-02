// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/preferences_helper.h"

#include <utility>

#include "base/bind.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/common/chrome_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/persistent_pref_store.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

using sync_datatype_helper::test;

namespace preferences_helper {

PrefService* GetPrefs(int index) {
  return test()->GetProfile(index)->GetPrefs();
}

user_prefs::PrefRegistrySyncable* GetRegistry(Profile* profile) {
  // TODO(tschumann): Not sure what's the cleanest way to avoid this deprecated
  // call is. Ideally we could use a servicification integration test.
  // Another option would be to have a ForTest-only variant of
  // KeyedServiceBaseFactory::GetAssociatedPrefRegistry().
  return static_cast<user_prefs::PrefRegistrySyncable*>(
      profile->GetPrefs()->DeprecatedGetPrefRegistry());
}

void ChangeBooleanPref(int index, const char* pref_name) {
  bool new_value = !GetPrefs(index)->GetBoolean(pref_name);
  GetPrefs(index)->SetBoolean(pref_name, new_value);
}

void ChangeIntegerPref(int index, const char* pref_name, int new_value) {
  GetPrefs(index)->SetInteger(pref_name, new_value);
}

void ChangeStringPref(int index,
                      const char* pref_name,
                      const std::string& new_value) {
  GetPrefs(index)->SetString(pref_name, new_value);
}

void ClearPref(int index, const char* pref_name) {
  GetPrefs(index)->ClearPref(pref_name);
}

void ChangeListPref(int index,
                    const char* pref_name,
                    const base::ListValue& new_value) {
  ListPrefUpdate update(GetPrefs(index), pref_name);
  base::ListValue* list = update.Get();
  for (const auto& it : new_value) {
    list->Append(it.CreateDeepCopy());
  }
}

scoped_refptr<PrefStore> BuildPrefStoreFromPrefsFile(Profile* profile) {
  base::RunLoop run_loop;
  profile->GetPrefs()->CommitPendingWrite(run_loop.QuitClosure());
  run_loop.Run();

  auto pref_store = base::MakeRefCounted<JsonPrefStore>(
      profile->GetPath().Append(chrome::kPreferencesFilename));
  base::ScopedAllowBlockingForTesting allow_blocking;
  if (pref_store->ReadPrefs() != PersistentPrefStore::PREF_READ_ERROR_NONE) {
    ADD_FAILURE() << " Failed reading the prefs file into the store.";
  }

  return pref_store;
}

bool BooleanPrefMatches(const char* pref_name) {
  bool reference_value = GetPrefs(0)->GetBoolean(pref_name);
  for (int i = 1; i < test()->num_clients(); ++i) {
    if (reference_value != GetPrefs(i)->GetBoolean(pref_name)) {
      DVLOG(1) << "Boolean preference " << pref_name << " mismatched in"
               << " profile " << i << ".";
      return false;
    }
  }
  return true;
}

bool IntegerPrefMatches(const char* pref_name) {
  int reference_value = GetPrefs(0)->GetInteger(pref_name);
  for (int i = 1; i < test()->num_clients(); ++i) {
    if (reference_value != GetPrefs(i)->GetInteger(pref_name)) {
      DVLOG(1) << "Integer preference " << pref_name << " mismatched in"
               << " profile " << i << ".";
      return false;
    }
  }
  return true;
}

bool StringPrefMatches(const char* pref_name) {
  std::string reference_value = GetPrefs(0)->GetString(pref_name);
  for (int i = 1; i < test()->num_clients(); ++i) {
    if (reference_value != GetPrefs(i)->GetString(pref_name)) {
      DVLOG(1) << "String preference " << pref_name << " mismatched in"
               << " profile " << i << ".";
      return false;
    }
  }
  return true;
}

bool ClearedPrefMatches(const char* pref_name) {
  for (int i = 0; i < test()->num_clients(); ++i) {
    if (GetPrefs(i)->GetUserPrefValue(pref_name)) {
      DVLOG(1) << "Preference " << pref_name << " isn't cleared in"
               << " profile " << i << ".";
      return false;
    }
  }
  return true;
}

bool ListPrefMatches(const char* pref_name) {
  const base::ListValue* reference_value = GetPrefs(0)->GetList(pref_name);
  for (int i = 1; i < test()->num_clients(); ++i) {
    if (!reference_value->Equals(GetPrefs(i)->GetList(pref_name))) {
      DVLOG(1) << "List preference " << pref_name << " mismatched in"
               << " profile " << i << ".";
      return false;
    }
  }
  return true;
}

base::Optional<sync_pb::PreferenceSpecifics> GetPreferenceInFakeServer(
    const std::string& pref_name,
    fake_server::FakeServer* fake_server) {
  for (const sync_pb::SyncEntity& entity :
       fake_server->GetSyncEntitiesByModelType(syncer::PREFERENCES)) {
    if (entity.specifics().preference().name() == pref_name) {
      return entity.specifics().preference();
    }
  }

  return base::nullopt;
}

}  // namespace preferences_helper

PrefMatchChecker::PrefMatchChecker(const char* path) : path_(path) {
  for (int i = 0; i < test()->num_clients(); ++i) {
    RegisterPrefListener(preferences_helper::GetPrefs(i));
  }
}

PrefMatchChecker::~PrefMatchChecker() = default;

const char* PrefMatchChecker::GetPath() const {
  return path_;
}

void PrefMatchChecker::RegisterPrefListener(PrefService* pref_service) {
  std::unique_ptr<PrefChangeRegistrar> registrar(new PrefChangeRegistrar());
  registrar->Init(pref_service);
  registrar->Add(path_,
                 base::BindRepeating(&PrefMatchChecker::CheckExitCondition,
                                     base::Unretained(this)));
  pref_change_registrars_.push_back(std::move(registrar));
}

ListPrefMatchChecker::ListPrefMatchChecker(const char* path)
    : PrefMatchChecker(path) {}

bool ListPrefMatchChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for pref '" << GetPath() << "' to match";
  return preferences_helper::ListPrefMatches(GetPath());
}

BooleanPrefMatchChecker::BooleanPrefMatchChecker(const char* path)
    : PrefMatchChecker(path) {}

bool BooleanPrefMatchChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for pref '" << GetPath() << "' to match";
  return preferences_helper::BooleanPrefMatches(GetPath());
}

IntegerPrefMatchChecker::IntegerPrefMatchChecker(const char* path)
    : PrefMatchChecker(path) {}

bool IntegerPrefMatchChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for pref '" << GetPath() << "' to match";
  return preferences_helper::IntegerPrefMatches(GetPath());
}

StringPrefMatchChecker::StringPrefMatchChecker(const char* path)
    : PrefMatchChecker(path) {}

bool StringPrefMatchChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for pref '" << GetPath() << "' to match";
  return preferences_helper::StringPrefMatches(GetPath());
}

ClearedPrefMatchChecker::ClearedPrefMatchChecker(const char* path)
    : PrefMatchChecker(path) {}

bool ClearedPrefMatchChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for pref '" << GetPath() << "' to match";
  return preferences_helper::ClearedPrefMatches(GetPath());
}

FakeServerPrefMatchesValueChecker::FakeServerPrefMatchesValueChecker(
    const std::string& pref_name,
    const std::string& expected_value)
    : pref_name_(pref_name), expected_value_(expected_value) {}

bool FakeServerPrefMatchesValueChecker::IsExitConditionSatisfied(
    std::ostream* os) {
  const base::Optional<sync_pb::PreferenceSpecifics> actual_specifics =
      preferences_helper::GetPreferenceInFakeServer(pref_name_, fake_server());
  if (!actual_specifics.has_value()) {
    *os << "No sync entity in FakeServer for pref " << pref_name_;
    return false;
  }

  *os << "Waiting until FakeServer value for pref " << pref_name_ << " becomes "
      << expected_value_ << " but actual is " << actual_specifics->value();
  return actual_specifics->value() == expected_value_;
}
