// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_PREFERENCES_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_PREFERENCES_HELPER_H_

#include <stdint.h>

#include "base/optional.h"

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "chrome/browser/sync/test/integration/fake_server_match_status_checker.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "components/prefs/json_pref_store.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/test/fake_server/fake_server.h"

class PrefChangeRegistrar;
class PrefService;
class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace preferences_helper {

// Used to access the preferences within a particular sync profile.
PrefService* GetPrefs(int index);

// Provides access to the syncable pref registy of a profile.
user_prefs::PrefRegistrySyncable* GetRegistry(Profile* profile);

// Inverts the value of the boolean preference with name |pref_name| in the
// profile with index |index|.
void ChangeBooleanPref(int index, const char* pref_name);

// Changes the value of the integer preference with name |pref_name| in the
// profile with index |index| to |new_value|.
void ChangeIntegerPref(int index, const char* pref_name, int new_value);

// Changes the value of the string preference with name |pref_name| in the
// profile with index |index| to |new_value|.
void ChangeStringPref(int index,
                      const char* pref_name,
                      const std::string& new_value);

// Clears the value of the preference with name |pref_name| in the profile with
// index |index|.
void ClearPref(int index, const char* pref_name);

// Changes the value of the list preference with name |pref_name| in the
// profile with index |index| to |new_value|.
void ChangeListPref(int index,
                    const char* pref_name,
                    const base::ListValue& new_value);

// Reads preferences from a given profile's pref file (after flushing) and loads
// them into a new created pref store.
scoped_refptr<PrefStore> BuildPrefStoreFromPrefsFile(Profile* profile);

// Used to verify that the boolean preference with name |pref_name| has the
// same value across all profiles.
bool BooleanPrefMatches(const char* pref_name) WARN_UNUSED_RESULT;

// Used to verify that the integer preference with name |pref_name| has the
// same value across all profiles.
bool IntegerPrefMatches(const char* pref_name) WARN_UNUSED_RESULT;

// Used to verify that the string preference with name |pref_name| has the
// same value across all profiles.
bool StringPrefMatches(const char* pref_name) WARN_UNUSED_RESULT;

// Used to verify that the list preference with name |pref_name| has the
// same value across all profiles.
bool ListPrefMatches(const char* pref_name) WARN_UNUSED_RESULT;

// Returns a server-side preference in FakeServer for |pref_name| or nullopt if
// no preference exists.
base::Optional<sync_pb::PreferenceSpecifics> GetPreferenceInFakeServer(
    const std::string& pref_name,
    fake_server::FakeServer* fake_server);

}  // namespace preferences_helper

// Abstract checker that takes care of registering for preference changes.
class PrefMatchChecker : public StatusChangeChecker {
 public:
  explicit PrefMatchChecker(const char* path);
  ~PrefMatchChecker() override;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override = 0;

 protected:
  const char* GetPath() const;

 private:
  void RegisterPrefListener(PrefService* pref_service);

  std::vector<std::unique_ptr<PrefChangeRegistrar>> pref_change_registrars_;
  const char* path_;
};

// Matcher that blocks until the specified list pref matches on all clients.
class ListPrefMatchChecker : public PrefMatchChecker {
 public:
  explicit ListPrefMatchChecker(const char* path);

  // PrefMatchChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;
};

// Matcher that blocks until the specified boolean pref matches on all clients.
class BooleanPrefMatchChecker : public PrefMatchChecker {
 public:
  explicit BooleanPrefMatchChecker(const char* path);

  // PrefMatchChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;
};

// Matcher that blocks until the specified integer pref matches on all clients.
class IntegerPrefMatchChecker : public PrefMatchChecker {
 public:
  explicit IntegerPrefMatchChecker(const char* path);

  // PrefMatchChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;
};

// Matcher that blocks until the specified string pref matches on all clients.
class StringPrefMatchChecker : public PrefMatchChecker {
 public:
  explicit StringPrefMatchChecker(const char* path);

  // PrefMatchChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;
};

// Matcher that blocks until the specified pref is cleared on all clients.
class ClearedPrefMatchChecker : public PrefMatchChecker {
 public:
  explicit ClearedPrefMatchChecker(const char* path);

  // PrefMatchChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;
};

// Waits until GetPreferenceInFakeServer() returns an expected value.
class FakeServerPrefMatchesValueChecker
    : public fake_server::FakeServerMatchStatusChecker {
 public:
  FakeServerPrefMatchesValueChecker(const std::string& pref_name,
                                    const std::string& expected_value);

 protected:
  // StatusChangeChecker overrides.
  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  const std::string pref_name_;
  const std::string expected_value_;
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_PREFERENCES_HELPER_H_
