// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_PASSWORDS_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_PASSWORDS_HELPER_H_

#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/fake_server_match_status_checker.h"
#include "chrome/browser/sync/test/integration/multi_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/password_manager/core/browser/password_form.h"

namespace syncer {
class Cryptographer;
class KeyDerivationParams;
}

namespace password_manager {
class PasswordStoreInterface;
}

namespace passwords_helper {

// Returns all logins from |store| matching a fake signon realm (see
// CreateTestPasswordForm()).
// TODO(treib): Rename this to make clear how specific it is.
std::vector<std::unique_ptr<password_manager::PasswordForm>> GetLogins(
    password_manager::PasswordStoreInterface* store);

// Returns all logins from |store| (including blocklisted ones)
std::vector<std::unique_ptr<password_manager::PasswordForm>> GetAllLogins(
    password_manager::PasswordStoreInterface* store);

// Removes all password forms from the password store |store|. This is an async
// method that return immediately and does *not* block until the operation is
// finished on the background thread.
void RemoveLogins(password_manager::PasswordStoreInterface* store);

// Gets the password store of the profile with index |index|.
password_manager::PasswordStoreInterface* GetProfilePasswordStoreInterface(
    int index);

// Gets the password store of the verifier profile.
password_manager::PasswordStoreInterface*
GetVerifierProfilePasswordStoreInterface();

// Gets the account-scoped password store of the profile with index |index|.
password_manager::PasswordStoreInterface* GetAccountPasswordStoreInterface(
    int index);

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
  explicit PasswordSyncActiveChecker(syncer::SyncServiceImpl* service);
  ~PasswordSyncActiveChecker() override;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;
};

// TODO(crbug.com/1010490): avoid re-entrance protection in checkers below or
// factor it out to not duplicate in every checker.
// Checker to block until all profiles contain the same password forms.
class SamePasswordFormsChecker : public MultiClientStatusChangeChecker {
 public:
  SamePasswordFormsChecker();
  ~SamePasswordFormsChecker() override;
  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  bool in_progress_ = false;
  bool needs_recheck_ = false;
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

// Checker to block until server has the given password forms encrypted with
// given encryption params.
class ServerPasswordsEqualityChecker
    : public fake_server::FakeServerMatchStatusChecker {
 public:
  ServerPasswordsEqualityChecker(
      const std::vector<password_manager::PasswordForm>& expected_forms,
      const std::string& encryption_passphrase,
      const syncer::KeyDerivationParams& key_derivation_params);
  ~ServerPasswordsEqualityChecker() override;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  const std::unique_ptr<syncer::Cryptographer> cryptographer_;

  std::vector<std::unique_ptr<password_manager::PasswordForm>> expected_forms_;
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_PASSWORDS_HELPER_H_
