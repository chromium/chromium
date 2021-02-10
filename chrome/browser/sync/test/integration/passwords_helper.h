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
#include "components/password_manager/core/browser/insecure_credentials_table.h"
#include "components/password_manager/core/browser/password_form.h"

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
              const password_manager::PasswordForm& form);

// Adds |issue| to the password store |store|.
void AddInsecureCredential(password_manager::PasswordStore* store,
                           const password_manager::InsecureCredential& issue);

// Update the data held in password store |store| with a modified |form|.
// This method blocks until the operation is complete.
void UpdateLogin(password_manager::PasswordStore* store,
                 const password_manager::PasswordForm& form);

// Removes |old_form| from password store |store| and immediately adds
// |new_form|. This method blocks until the operation is complete.
void UpdateLoginWithPrimaryKey(password_manager::PasswordStore* store,
                               const password_manager::PasswordForm& new_form,
                               const password_manager::PasswordForm& old_form);

// Returns all logins from |store| matching a fake signon realm (see
// CreateTestPasswordForm()).
// TODO(treib): Rename this to make clear how specific it is.
std::vector<std::unique_ptr<password_manager::PasswordForm>> GetLogins(
    password_manager::PasswordStore* store);

// Returns all insecure credentials from |store|.
std::vector<password_manager::InsecureCredential> GetAllInsecureCredentials(
    password_manager::PasswordStore* store);

// Returns all logins from |store| (including blocklisted ones)
std::vector<std::unique_ptr<password_manager::PasswordForm>> GetAllLogins(
    password_manager::PasswordStore* store);

// Removes the login held in |form| from the password store |store|.  This
// method blocks until the operation is complete.
void RemoveLogin(password_manager::PasswordStore* store,
                 const password_manager::PasswordForm& form);

// Removes all password forms from the password store |store|.
void RemoveLogins(password_manager::PasswordStore* store);

// Removes passed insecure credential from the |store|.
void RemoveInsecureCredentials(
    password_manager::PasswordStore* store,
    const password_manager::InsecureCredential& credential);

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
password_manager::PasswordForm CreateTestPasswordForm(int index);

// Creates a test insecure credentials with a well known fake signon realm
// and username based on |index|. Implementation aligned with
// CreateTestPasswordForm(int index);
password_manager::InsecureCredential CreateInsecureCredential(
    int index,
    password_manager::InsecureType type);

// Injects the password entity based on given |form| and encrypted with key
// derived from |key_derivation_params| into |fake_server|.
// For Keystore encryption, the |encryption_passphrase| is the base64 encoding
// of FakeServer::GetKeystoreKeys().back().
void InjectEncryptedServerPassword(
    const password_manager::PasswordForm& form,
    const std::string& encryption_passphrase,
    const syncer::KeyDerivationParams& key_derivation_params,
    fake_server::FakeServer* fake_server);
// As above, but takes a PasswordSpecificsData instead of a PasswordForm.
void InjectEncryptedServerPassword(
    const sync_pb::PasswordSpecificsData& password_data,
    const std::string& encryption_passphrase,
    const syncer::KeyDerivationParams& key_derivation_params,
    fake_server::FakeServer* fake_server);
// As above, but using standard Keystore encryption.
void InjectKeystoreEncryptedServerPassword(
    const password_manager::PasswordForm& form,
    fake_server::FakeServer* fake_server);
// As above, but using standard Keystore encryption and PasswordSpecificsData.
void InjectKeystoreEncryptedServerPassword(
    const sync_pb::PasswordSpecificsData& password_data,
    fake_server::FakeServer* fake_server);

}  // namespace passwords_helper

// Checker to wait until the PASSWORDS datatype becomes active.
class PasswordSyncActiveChecker : public SingleClientStatusChangeChecker {
 public:
  explicit PasswordSyncActiveChecker(syncer::ProfileSyncService* service);
  ~PasswordSyncActiveChecker() override;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;
};

// TODO(crbug.com/1010490): avoid re-entrance protection in checkers below or
// factor it out to not duplicate in every checker.
// Checker to block until all profiles contain the same password forms.
// If |check_for_insecure_| is true, it checks that all profiles contains the
// same insecure credentials too.
class SamePasswordFormsChecker : public MultiClientStatusChangeChecker {
 public:
  using CheckForInsecure = base::StrongAlias<class CheckForInsecureTag, bool>;

  SamePasswordFormsChecker();
  explicit SamePasswordFormsChecker(CheckForInsecure check_for_insecure);
  ~SamePasswordFormsChecker() override;
  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  bool in_progress_ = false;
  bool needs_recheck_ = false;
  CheckForInsecure check_for_insecure_{false};
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
      const std::vector<password_manager::PasswordForm>& expected_forms);
  ~PasswordFormsChecker() override;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  bool IsExitConditionSatisfiedImpl(std::ostream* os);

  const int index_;
  std::vector<std::unique_ptr<password_manager::PasswordForm>> expected_forms_;
  bool in_progress_;
  bool needs_recheck_;
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_PASSWORDS_HELPER_H_
