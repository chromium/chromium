// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/preferences_helper.h"

#include <utility>

#include "base/strings/stringprintf.h"
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

PrefService* GetVerifierPrefs() {
  return test()->verifier()->GetPrefs();
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
  if (test()->use_verifier())
    GetVerifierPrefs()->SetBoolean(pref_name, new_value);
}

void ChangeIntegerPref(int index, const char* pref_name, int new_value) {
  GetPrefs(index)->SetInteger(pref_name, new_value);
  if (test()->use_verifier())
    GetVerifierPrefs()->SetInteger(pref_name, new_value);
}

void ChangeInt64Pref(int index, const char* pref_name, int64_t new_value) {
  GetPrefs(index)->SetInt64(pref_name, new_value);
  if (test()->use_verifier())
    GetVerifierPrefs()->SetInt64(pref_name, new_value);
}

void ChangeDoublePref(int index, const char* pref_name, double new_value) {
  GetPrefs(index)->SetDouble(pref_name, new_value);
  if (test()->use_verifier())
    GetVerifierPrefs()->SetDouble(pref_name, new_value);
}

void ChangeStringPref(int index,
                      const char* pref_name,
                      const std::string& new_value) {
  GetPrefs(index)->SetString(pref_name, new_value);
  if (test()->use_verifier())
    GetVerifierPrefs()->SetString(pref_name, new_value);
}

void ChangeFilePathPref(int index,
                        const char* pref_name,
                        const base::FilePath& new_value) {
  GetPrefs(index)->SetFilePath(pref_name, new_value);
  if (test()->use_verifier())
    GetVerifierPrefs()->SetFilePath(pref_name, new_value);
}

void ChangeListPref(int index,
                    const char* pref_name,
                    const base::ListValue& new_value) {
  {
    ListPrefUpdate update(GetPrefs(index), pref_name);
    base::ListValue* list = update.Get();
    for (auto it = new_value.begin(); it != new_value.end(); ++it) {
      list->Append(it->CreateDeepCopy());
    }
  }

  if (test()->use_verifier()) {
    ListPrefUpdate update_verifier(GetVerifierPrefs(), pref_name);
    base::ListValue* list_verifier = update_verifier.Get();
    for (auto it = new_value.begin(); it != new_value.end(); ++it) {
      list_verifier->Append(it->CreateDeepCopy());
    }
  }
}

scoped_refptr<PrefStore> BuildPrefStoreFromPrefsFile(Profile* profile) {
  profile->GetPrefs()->CommitPendingWrite();
  // Writes are scheduled on the IO thread. The JsonPrefStore requires all
  // access (construction, Get, Set, ReadPrefs) to be made from the same thread.
  // So instead of reading the file from the IO thread, we simply schedule a
  // dummy task to avoid races with writing the file and reading it.
  base::RunLoop run_loop;
  profile->GetIOTaskRunner()->PostTask(FROM_HERE,
                                       base::BindOnce(run_loop.QuitClosure()));
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
  bool reference_value;
  if (test()->use_verifier()) {
    reference_value = GetVerifierPrefs()->GetBoolean(pref_name);
  } else {
    reference_value = GetPrefs(0)->GetBoolean(pref_name);
  }
  for (int i = 0; i < test()->num_clients(); ++i) {
    if (reference_value != GetPrefs(i)->GetBoolean(pref_name)) {
      DVLOG(1) << "Boolean preference " << pref_name << " mismatched in"
               << " profile " << i << ".";
      return false;
    }
  }
  return true;
}

bool IntegerPrefMatches(const char* pref_name) {
  int reference_value;
  if (test()->use_verifier()) {
    reference_value = GetVerifierPrefs()->GetInteger(pref_name);
  } else {
    reference_value = GetPrefs(0)->GetInteger(pref_name);
  }
  for (int i = 0; i < test()->num_clients(); ++i) {
    if (reference_value != GetPrefs(i)->GetInteger(pref_name)) {
      DVLOG(1) << "Integer preference " << pref_name << " mismatched in"
               << " profile " << i << ".";
      return false;
    }
  }
  return true;
}

bool Int64PrefMatches(const char* pref_name) {
  int64_t reference_value;
  if (test()->use_verifier()) {
    reference_value = GetVerifierPrefs()->GetInt64(pref_name);
  } else {
    reference_value = GetPrefs(0)->GetInt64(pref_name);
  }
  for (int i = 0; i < test()->num_clients(); ++i) {
    if (reference_value != GetPrefs(i)->GetInt64(pref_name)) {
      DVLOG(1) << "Integer preference " << pref_name << " mismatched in"
               << " profile " << i << ".";
      return false;
    }
  }
  return true;
}

bool DoublePrefMatches(const char* pref_name) {
  double reference_value;
  if (test()->use_verifier()) {
    reference_value = GetVerifierPrefs()->GetDouble(pref_name);
  } else {
    reference_value = GetPrefs(0)->GetDouble(pref_name);
  }
  for (int i = 0; i < test()->num_clients(); ++i) {
    if (reference_value != GetPrefs(i)->GetDouble(pref_name)) {
      DVLOG(1) << "Double preference " << pref_name << " mismatched in"
               << " profile " << i << ".";
      return false;
    }
  }
  return true;
}

bool StringPrefMatches(const char* pref_name) {
  std::string reference_value;
  if (test()->use_verifier()) {
    reference_value = GetVerifierPrefs()->GetString(pref_name);
  } else {
    reference_value = GetPrefs(0)->GetString(pref_name);
  }
  for (int i = 0; i < test()->num_clients(); ++i) {
    if (reference_value != GetPrefs(i)->GetString(pref_name)) {
      DVLOG(1) << "String preference " << pref_name << " mismatched in"
               << " profile " << i << ".";
      return false;
    }
  }
  return true;
}

bool FilePathPrefMatches(const char* pref_name) {
  base::FilePath reference_value;
  if (test()->use_verifier()) {
    reference_value = GetVerifierPrefs()->GetFilePath(pref_name);
  } else {
    reference_value = GetPrefs(0)->GetFilePath(pref_name);
  }
  for (int i = 0; i < test()->num_clients(); ++i) {
    if (reference_value != GetPrefs(i)->GetFilePath(pref_name)) {
      DVLOG(1) << "base::FilePath preference " << pref_name << " mismatched in"
               << " profile " << i << ".";
      return false;
    }
  }
  return true;
}

bool ListPrefMatches(const char* pref_name) {
  const base::ListValue* reference_value;
  if (test()->use_verifier()) {
    reference_value = GetVerifierPrefs()->GetList(pref_name);
  } else {
    reference_value = GetPrefs(0)->GetList(pref_name);
  }
  for (int i = 0; i < test()->num_clients(); ++i) {
    if (!reference_value->Equals(GetPrefs(i)->GetList(pref_name))) {
      DVLOG(1) << "List preference " << pref_name << " mismatched in"
               << " profile " << i << ".";
      return false;
    }
  }
  return true;
}

}  // namespace preferences_helper

PrefMatchChecker::PrefMatchChecker(const char* path) : path_(path) {
  if (test()->use_verifier()) {
    RegisterPrefListener(preferences_helper::GetVerifierPrefs());
  }
  for (int i = 0; i < test()->num_clients(); ++i) {
    RegisterPrefListener(preferences_helper::GetPrefs(i));
  }
}

PrefMatchChecker::~PrefMatchChecker() {}

std::string PrefMatchChecker::GetDebugMessage() const {
  return base::StringPrintf("Waiting for pref '%s' to match", GetPath());
}

const char* PrefMatchChecker::GetPath() const {
  return path_;
}

void PrefMatchChecker::RegisterPrefListener(PrefService* pref_service) {
  std::unique_ptr<PrefChangeRegistrar> registrar(new PrefChangeRegistrar());
  registrar->Init(pref_service);
  registrar->Add(path_,
                 base::Bind(&PrefMatchChecker::CheckExitCondition,
                            base::Unretained(this)));
  pref_change_registrars_.push_back(std::move(registrar));
}

ListPrefMatchChecker::ListPrefMatchChecker(const char* path)
    : PrefMatchChecker(path) {}

bool ListPrefMatchChecker::IsExitConditionSatisfied() {
  return preferences_helper::ListPrefMatches(GetPath());
}

BooleanPrefMatchChecker::BooleanPrefMatchChecker(const char* path)
    : PrefMatchChecker(path) {}

bool BooleanPrefMatchChecker::IsExitConditionSatisfied() {
  return preferences_helper::BooleanPrefMatches(GetPath());
}

IntegerPrefMatchChecker::IntegerPrefMatchChecker(const char* path)
    : PrefMatchChecker(path) {}

bool IntegerPrefMatchChecker::IsExitConditionSatisfied() {
  return preferences_helper::IntegerPrefMatches(GetPath());
}

StringPrefMatchChecker::StringPrefMatchChecker(const char* path)
    : PrefMatchChecker(path) {}

bool StringPrefMatchChecker::IsExitConditionSatisfied() {
  return preferences_helper::StringPrefMatches(GetPath());
}
