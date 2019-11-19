// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_PASSWORDS_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_PASSWORDS_HELPER_H_

#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/multi_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/autofill/core/common/password_form.h"

namespace syncer {
class KeyDerivationParams;
}

namespace password_manager {
class PasswordStore;
}

namespace passwords_helper {

// Adds the login held in |form| to the password store |store|. Even though
// logins are normally added asynchronously, this method will block until the
// login is added.
void AddLogin(password_manager::PasswordStore* store,
              const autofill::PasswordForm& form);

// Update the data held in password store |store| with a modified |form|.
// This method blocks until the operation is complete.
void UpdateLogin(password_manager::PasswordStore* store,
                 const autofill::PasswordForm& form);

// Removes |old_form| from password store |store| and immediately adds
// |new_form|. This method blocks until the operation is complete.
void UpdateLoginWithPrimaryKey(password_manager::PasswordStore* store,
                               const autofill::PasswordForm& new_form,
                               const autofill::PasswordForm& old_form);

// Returns all logins from |store| matching a fake signon realm (see
// CreateTestPasswordForm()).
// TODO(treib): Rename this to make clear how specific it is.
std::vector<std::unique_ptr<autofill::PasswordForm>> GetLogins(
    password_manager::PasswordStore* store);

// Returns all logins from |store| (including blacklisted ones)
std::vector<std::unique_ptr<autofill::PasswordForm>> GetAllLogins(
    password_manager::PasswordStore* store);

// Removes the login held in |form| from the password store |store|.  This
// method blocks until the operation is complete.
void RemoveLogin(password_manager::PasswordStore* store,
                 const autofill::PasswordForm& form);

// Removes all password forms from the password store |store|.
void RemoveLogins(password_manager::PasswordStore* store);

// Gets the password store of the profile with index |index|.
// TODO(treib): Rename to GetProfilePasswordStore.
password_manager::PasswordStore* GetPasswordStore(int index);

// Gets the password store of the verifier profile.
// TODO(treib): Rename to GetVerifierProfilePasswordStore.
password_manager::PasswordStore* GetVerifierPasswordStore();

// Gets the account-scoped password store of the profile with index |index|.
password_manager::PasswordStore* GetAccountPasswordStore(int index);

// Returns true iff the profile with index |index| contains the same password
// forms as the verifier profile.
bool ProfileContainsSamePasswordFormsAsVerifier(int index);

// Returns true iff the profile with index |index_a| contains the same
// password forms as the profile with index |index_b|.
bool ProfilesContainSamePasswordForms(int index_a, int index_b);

// Returns true iff all profiles contain the same password forms as the
// verifier profile.
bool AllProfilesContainSamePasswordFormsAsVerifier();

// Returns true iff all profiles contain the same password forms.
bool AllProfilesContainSamePasswordForms();

bool AwaitProfileContainsSamePasswordFormsAsVerifier(int index);

// Returns the number of forms in the password store of the profile with index
// |index|.
int GetPasswordCount(int index);

// Returns the number of forms in the password store of the verifier profile.
int GetVerifierPasswordCount();

// Creates a test password form with a well known fake signon realm based on
// |index|.
autofill::PasswordForm CreateTestPasswordForm(int index);

// Injects the password entity based on given |form| and encrypted with key
// derived from |key_params| into |fake_server|.
void InjectEncryptedServerPassword(
    const autofill::PasswordForm& form,
    const std::string& encryption_passphrase,
    const syncer::KeyDerivationParams& key_derivation_params,
    fake_server::FakeServer* fake_server);

}  // namespace passwords_helper

// TODO(crbug.com/1010490): avoid re-entrance protection in checkers below or
// factor it out to not duplicate in every checker.
// Checker to block until all profiles contain the same password forms.
class SamePasswordFormsChecker : public MultiClientStatusChangeChecker {
 public:
  SamePasswordFormsChecker();

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  bool in_progress_;
  bool needs_recheck_;
};

// Checker to block until specified profile contains the same password forms as
// the verifier.
class SamePasswordFormsAsVerifierChecker
    : public SingleClientStatusChangeChecker {
 public:
  explicit SamePasswordFormsAsVerifierChecker(int index);

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  int index_;
  bool in_progress_;
  bool needs_recheck_;
};

// Checker to block until specified profile contains the given password forms.
class PasswordFormsChecker : public SingleClientStatusChangeChecker {
 public:
  PasswordFormsChecker(
      int index,
      const std::vector<autofill::PasswordForm>& expected_forms);
  ~PasswordFormsChecker() override;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  bool IsExitConditionSatisfiedImpl(std::ostream* os);

  const int index_;
  std::vector<std::unique_ptr<autofill::PasswordForm>> expected_forms_;
  bool in_progress_;
  bool needs_recheck_;
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_PASSWORDS_HELPER_H_
