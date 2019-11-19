// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_PREFERENCES_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_PREFERENCES_HELPER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "components/prefs/json_pref_store.h"

class PrefChangeRegistrar;
class PrefService;
class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace preferences_helper {

// Used to access the preferences within a particular sync profile.
PrefService* GetPrefs(int index);

// Used to access the preferences within the verifier sync profile.
PrefService* GetVerifierPrefs();

// Provides access to the syncable pref registy of a profile.
user_prefs::PrefRegistrySyncable* GetRegistry(Profile* profile);

// Inverts the value of the boolean preference with name |pref_name| in the
// profile with index |index|. Also inverts its value in |verifier| if
// DisableVerifier() hasn't been called.
void ChangeBooleanPref(int index, const char* pref_name);

// Changes the value of the integer preference with name |pref_name| in the
// profile with index |index| to |new_value|. Also changes its value in
// |verifier| if DisableVerifier() hasn't been called.
void ChangeIntegerPref(int index, const char* pref_name, int new_value);

// Changes the value of the int64_t preference with name |pref_name| in the
// profile with index |index| to |new_value|. Also changes its value in
// |verifier| if DisableVerifier() hasn't been called.
void ChangeInt64Pref(int index, const char* pref_name, int64_t new_value);

// Changes the value of the double preference with name |pref_name| in the
// profile with index |index| to |new_value|. Also changes its value in
// |verifier| if DisableVerifier() hasn't been called.
void ChangeDoublePref(int index, const char* pref_name, double new_value);

// Changes the value of the string preference with name |pref_name| in the
// profile with index |index| to |new_value|. Also changes its value in
// |verifier| if DisableVerifier() hasn't been called.
void ChangeStringPref(int index,
                      const char* pref_name,
                      const std::string& new_value);

// Clears the value of the preference with name |pref_name| in the profile with
// index |index|. Also changes its value in |verifier| if DisableVerifier()
// hasn't been called.
void ClearPref(int index, const char* pref_name);

// Changes the value of the file path preference with name |pref_name| in the
// profile with index |index| to |new_value|. Also changes its value in
// |verifier| if DisableVerifier() hasn't been called.
void ChangeFilePathPref(int index,
                        const char* pref_name,
                        const base::FilePath& new_value);

// Changes the value of the list preference with name |pref_name| in the
// profile with index |index| to |new_value|. Also changes its value in
// |verifier| if DisableVerifier() hasn't been called.
void ChangeListPref(int index,
                    const char* pref_name,
                    const base::ListValue& new_value);

// Reads preferences from a given profile's pref file (after flushing) and loads
// them into a new created pref store.
scoped_refptr<PrefStore> BuildPrefStoreFromPrefsFile(Profile* profile);

// Used to verify that the boolean preference with name |pref_name| has the
// same value across all profiles. Also checks |verifier| if DisableVerifier()
// hasn't been called.
bool BooleanPrefMatches(const char* pref_name) WARN_UNUSED_RESULT;

// Used to verify that the integer preference with name |pref_name| has the
// same value across all profiles. Also checks |verifier| if DisableVerifier()
// hasn't been called.
bool IntegerPrefMatches(const char* pref_name) WARN_UNUSED_RESULT;

// Used to verify that the int64_t preference with name |pref_name| has the
// same value across all profiles. Also checks |verifier| if DisableVerifier()
// hasn't been called.
bool Int64PrefMatches(const char* pref_name) WARN_UNUSED_RESULT;

// Used to verify that the double preference with name |pref_name| has the
// same value across all profiles. Also checks |verifier| if DisableVerifier()
// hasn't been called.
bool DoublePrefMatches(const char* pref_name) WARN_UNUSED_RESULT;

// Used to verify that the string preference with name |pref_name| has the
// same value across all profiles. Also checks |verifier| if DisableVerifier()
// hasn't been called.
bool StringPrefMatches(const char* pref_name) WARN_UNUSED_RESULT;

// Used to verify that the file path preference with name |pref_name| has the
// same value across all profiles. Also checks |verifier| if DisableVerifier()
// hasn't been called.
bool FilePathPrefMatches(const char* pref_name) WARN_UNUSED_RESULT;

// Used to verify that the list preference with name |pref_name| has the
// same value across all profiles. Also checks |verifier| if DisableVerifier()
// hasn't been called.
bool ListPrefMatches(const char* pref_name) WARN_UNUSED_RESULT;

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

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_PREFERENCES_HELPER_H_
