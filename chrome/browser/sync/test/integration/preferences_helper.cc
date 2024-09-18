// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/preferences_helper.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/notreached.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync/base/data_type.h"
#include "components/sync/protocol/sync_entity.pb.h"

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
                    const base::Value::List& new_value) {
  GetPrefs(index)->SetList(pref_name, new_value.Clone());
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
  const base::Value::List& reference_value = GetPrefs(0)->GetList(pref_name);
  for (int i = 1; i < test()->num_clients(); ++i) {
    if (reference_value != GetPrefs(i)->GetList(pref_name)) {
      DVLOG(1) << "List preference " << pref_name << " mismatched in"
               << " profile " << i << ".";
      return false;
    }
  }
  return true;
}

const sync_pb::PreferenceSpecifics& GetPreferenceFromEntity(
    syncer::DataType data_type,
    const sync_pb::SyncEntity& entity) {
  switch (data_type) {
    case syncer::PREFERENCES:
      return entity.specifics().preference();
    case syncer::PRIORITY_PREFERENCES:
      return entity.specifics().priority_preference().preference();
    case syncer::OS_PREFERENCES:
      return entity.specifics().os_preference().preference();
    case syncer::OS_PRIORITY_PREFERENCES:
      return entity.specifics().os_priority_preference().preference();
    default:
      NOTREACHED_IN_MIGRATION();
      return entity.specifics().preference();
  }
}

std::optional<sync_pb::PreferenceSpecifics> GetPreferenceInFakeServer(
    syncer::DataType data_type,
    const std::string& pref_name,
    fake_server::FakeServer* fake_server) {
  for (const sync_pb::SyncEntity& entity :
       fake_server->GetSyncEntitiesByDataType(data_type)) {
    const sync_pb::PreferenceSpecifics& preference =
        GetPreferenceFromEntity(data_type, entity);
    if (preference.name() == pref_name) {
      return preference;
    }
  }

  return std::nullopt;
}

std::string ConvertPrefValueToValueInSpecifics(const base::Value& value) {
  std::string result;
  bool success = base::JSONWriter::Write(value, &result);
  DCHECK(success);
  return result;
}

}  // namespace preferences_helper

PrefValueChecker::PrefValueChecker(PrefService* pref_service,
                                   const char* path,
                                   base::Value expected_value)
    : path_(path),
      expected_value_(std::move(expected_value)),
      pref_service_(pref_service) {
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      path_, base::BindRepeating(&PrefValueChecker::CheckExitCondition,
                                 base::Unretained(this)));
}

PrefValueChecker::~PrefValueChecker() = default;

bool PrefValueChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for pref '" << path_ << "' to be " << expected_value_;
  return pref_service_->GetValue(path_) == expected_value_;
}

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
    syncer::DataType data_type,
    const std::string& pref_name,
    const std::string& expected_value)
    : data_type_(data_type),
      pref_name_(pref_name),
      expected_value_(expected_value) {
  DCHECK(data_type_ == syncer::DataType::PREFERENCES ||
         data_type_ == syncer::DataType::PRIORITY_PREFERENCES ||
         data_type_ == syncer::DataType::OS_PREFERENCES ||
         data_type_ == syncer::DataType::OS_PRIORITY_PREFERENCES);
}

bool FakeServerPrefMatchesValueChecker::IsExitConditionSatisfied(
    std::ostream* os) {
  const std::optional<sync_pb::PreferenceSpecifics> actual_specifics =
      preferences_helper::GetPreferenceInFakeServer(data_type_, pref_name_,
                                                    fake_server());
  if (!actual_specifics.has_value()) {
    *os << "No sync entity in FakeServer for pref " << pref_name_;
    return false;
  }

  *os << "Waiting until FakeServer value for pref " << pref_name_ << " becomes "
      << expected_value_ << " but actual is " << actual_specifics->value();
  return actual_specifics->value() == expected_value_;
}
