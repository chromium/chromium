// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/passwords_helper.h"

#include <sstream>
#include <utility>

#include "base/base64.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/sync/password_proto_utils.h"
#include "components/sync/engine/loopback_server/persistent_unique_client_entity.h"
#include "components/sync/engine/nigori/key_derivation_params.h"
#include "components/sync/nigori/cryptographer_impl.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/password_specifics.pb.h"
#include "components/sync/service/sync_service_impl.h"
#include "content/public/test/test_utils.h"
#include "url/gurl.h"

using password_manager::PasswordForm;
using password_manager::PasswordStoreInterface;
using sync_datatype_helper::test;

namespace {

const char kFakeSignonRealm[] = "http://fake-signon-realm.google.com/";
const char kIndexedFakeOrigin[] = "http://fake-signon-realm.google.com/%d";

class PasswordStoreConsumerHelper
    : public password_manager::PasswordStoreConsumer {
 public:
  PasswordStoreConsumerHelper() = default;

  PasswordStoreConsumerHelper(const PasswordStoreConsumerHelper&) = delete;
  PasswordStoreConsumerHelper& operator=(const PasswordStoreConsumerHelper&) =
      delete;

  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>> results) override {
    result_.swap(results);
    run_loop_.Quit();
  }

  std::vector<std::unique_ptr<PasswordForm>> WaitForResult() {
    DCHECK(!run_loop_.running());
    run_loop_.Run();
    return std::move(result_);
  }

  base::WeakPtr<password_manager::PasswordStoreConsumer> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  // This RunLoop uses kNestableTasksAllowed because it runs nested within
  // another RunLoop.
  // TODO(crbug.com/41486990): consider changing this to PasswordStoreInterface
  // observer to avoid nested run loops.
  base::RunLoop run_loop_{base::RunLoop::Type::kNestableTasksAllowed};
  std::vector<std::unique_ptr<PasswordForm>> result_;
  base::WeakPtrFactory<PasswordStoreConsumerHelper> weak_ptr_factory_{this};
};

sync_pb::EntitySpecifics EncryptPasswordSpecifics(
    const sync_pb::PasswordSpecificsData& password_data,
    const std::string& passphrase,
    const syncer::KeyDerivationParams& key_derivation_params) {
  std::unique_ptr<syncer::CryptographerImpl> cryptographer =
      syncer::CryptographerImpl::FromSingleKeyForTesting(passphrase,
                                                         key_derivation_params);
  sync_pb::EntitySpecifics encrypted_specifics;
  encrypted_specifics.mutable_password()
      ->mutable_unencrypted_metadata()
      ->set_url(password_data.signon_realm());
  bool result = cryptographer->Encrypt(
      password_data,
      encrypted_specifics.mutable_password()->mutable_encrypted());
  DCHECK(result);
  return encrypted_specifics;
}

std::string GetClientTag(const sync_pb::PasswordSpecificsData& password_data) {
  return base::EscapePath(GURL(password_data.origin()).spec()) + "|" +
         base::EscapePath(password_data.username_element()) + "|" +
         base::EscapePath(password_data.username_value()) + "|" +
         base::EscapePath(password_data.password_element()) + "|" +
         base::EscapePath(password_data.signon_realm());
}

}  // namespace

namespace passwords_helper {

std::vector<std::unique_ptr<PasswordForm>> GetLogins(
    PasswordStoreInterface* store) {
  EXPECT_TRUE(store);
  PasswordStoreConsumerHelper consumer;
  store->GetAutofillableLogins(consumer.GetWeakPtr());
  return consumer.WaitForResult();
}

std::vector<std::unique_ptr<PasswordForm>> GetAllLogins(
    PasswordStoreInterface* store) {
  EXPECT_TRUE(store);
  PasswordStoreConsumerHelper consumer;
  store->GetAllLogins(consumer.GetWeakPtr());
  return consumer.WaitForResult();
}

void RemoveLogins(PasswordStoreInterface* store) {
  // Null Time values enforce unbounded deletion in both direction
  store->RemoveLoginsCreatedBetween(FROM_HERE,
                                    /*delete_begin=*/base::Time(),
                                    /*delete_end=*/base::Time::Max());
}
PasswordStoreInterface* GetProfilePasswordStoreInterface(int index) {
  return ProfilePasswordStoreFactory::GetForProfile(
             test()->GetProfile(index), ServiceAccessType::IMPLICIT_ACCESS)
      .get();
}

PasswordStoreInterface* GetVerifierProfilePasswordStoreInterface() {
  return ProfilePasswordStoreFactory::GetForProfile(
             test()->verifier(), ServiceAccessType::IMPLICIT_ACCESS)
      .get();
}

PasswordStoreInterface* GetAccountPasswordStoreInterface(int index) {
  return AccountPasswordStoreFactory::GetForProfile(
             test()->GetProfile(index), ServiceAccessType::IMPLICIT_ACCESS)
      .get();
}

password_manager::PasswordStoreInterface* GetPasswordStoreInterface(
    int index,
    PasswordForm::Store store) {
  switch (store) {
    case PasswordForm::Store::kNotSet:
      NOTREACHED();
    case PasswordForm::Store::kProfileStore:
      return GetProfilePasswordStoreInterface(index);
    case PasswordForm::Store::kAccountStore:
      return GetAccountPasswordStoreInterface(index);
  }
}

bool ProfileContainsSamePasswordFormsAsVerifier(int index) {
  std::vector<std::unique_ptr<PasswordForm>> verifier_forms =
      GetLogins(GetVerifierProfilePasswordStoreInterface());
  std::vector<std::unique_ptr<PasswordForm>> forms =
      GetLogins(GetProfilePasswordStoreInterface(index));

  std::ostringstream mismatch_details_stream;
  bool is_matching = password_manager::ContainsEqualPasswordFormsUnordered(
      verifier_forms, forms, &mismatch_details_stream);
  if (!is_matching) {
    VLOG(1) << "Profile " << index
            << " does not contain the same Password forms as Verifier Profile.";
    VLOG(1) << mismatch_details_stream.str();
  }
  return is_matching;
}

bool ProfilesContainSamePasswordForms(int index_a,
                                      int index_b,
                                      PasswordForm::Store store) {
  std::vector<std::unique_ptr<PasswordForm>> forms_a =
      GetLogins(GetPasswordStoreInterface(index_a, store));
  std::vector<std::unique_ptr<PasswordForm>> forms_b =
      GetLogins(GetPasswordStoreInterface(index_b, store));

  std::ostringstream mismatch_details_stream;
  bool is_matching = password_manager::ContainsEqualPasswordFormsUnordered(
      forms_a, forms_b, &mismatch_details_stream);
  if (!is_matching) {
    VLOG(1) << "Password forms in Profile " << index_a
            << " (listed as 'expected forms' below)"
            << " do not match those in Profile " << index_b
            << " (listed as 'actual forms' below)";
    VLOG(1) << mismatch_details_stream.str();
  }
  return is_matching;
}

bool AllProfilesContainSamePasswordFormsAsVerifier() {
  for (int i = 0; i < test()->num_clients(); ++i) {
    if (!ProfileContainsSamePasswordFormsAsVerifier(i)) {
      DVLOG(1) << "Profile " << i
               << " does not contain the same password"
                  " forms as the verifier.";
      return false;
    }
  }
  return true;
}

bool AllProfilesContainSamePasswordForms(PasswordForm::Store store) {
  for (int i = 1; i < test()->num_clients(); ++i) {
    if (!ProfilesContainSamePasswordForms(0, i, store)) {
      DVLOG(1) << "Profile " << i
               << " does not contain the same password"
                  " forms as Profile 0.";
      return false;
    }
  }
  return true;
}

int GetPasswordCount(int index, PasswordForm::Store store) {
  return GetLogins(GetPasswordStoreInterface(index, store)).size();
}

int GetVerifierPasswordCount() {
  return GetLogins(GetVerifierProfilePasswordStoreInterface()).size();
}

PasswordForm CreateTestPasswordForm(int index) {
  PasswordForm form;
  form.signon_realm = kFakeSignonRealm;
  form.url = GURL(base::StringPrintf(kIndexedFakeOrigin, index));
  form.username_value =
      base::ASCIIToUTF16(base::StringPrintf("username%d", index));
  form.password_value =
      base::ASCIIToUTF16(base::StringPrintf("password%d", index));
  form.date_created = base::Time::Now();
  form.in_store = password_manager::PasswordForm::Store::kProfileStore;
  return form;
}

void InjectEncryptedServerPassword(
    const password_manager::PasswordForm& form,
    const std::string& encryption_passphrase,
    const syncer::KeyDerivationParams& key_derivation_params,
    fake_server::FakeServer* fake_server) {
  sync_pb::PasswordSpecificsData password_data =
      password_manager::SpecificsFromPassword(form, /*base_password_data=*/{})
          .client_only_encrypted_data();
  InjectEncryptedServerPassword(password_data, encryption_passphrase,
                                key_derivation_params, fake_server);
}

void InjectEncryptedServerPassword(
    const sync_pb::PasswordSpecificsData& password_data,
    const std::string& encryption_passphrase,
    const syncer::KeyDerivationParams& key_derivation_params,
    fake_server::FakeServer* fake_server) {
  DCHECK(fake_server);
  const sync_pb::EntitySpecifics encrypted_specifics = EncryptPasswordSpecifics(
      password_data, encryption_passphrase, key_derivation_params);
  fake_server->InjectEntity(
      syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          /*non_unique_name=*/"encrypted", GetClientTag(password_data),
          encrypted_specifics,
          /*creation_time=*/0, /*last_modified_time=*/0));
}

void InjectKeystoreEncryptedServerPassword(
    const password_manager::PasswordForm& form,
    fake_server::FakeServer* fake_server) {
  sync_pb::PasswordSpecificsData password_data =
      password_manager::SpecificsFromPassword(form, /*base_password_data=*/{})
          .client_only_encrypted_data();
  InjectKeystoreEncryptedServerPassword(password_data, fake_server);
}

void InjectKeystoreEncryptedServerPassword(
    const sync_pb::PasswordSpecificsData& password_data,
    fake_server::FakeServer* fake_server) {
  InjectEncryptedServerPassword(
      password_data, base::Base64Encode(fake_server->GetKeystoreKeys().back()),
      syncer::KeyDerivationParams::CreateForPbkdf2(), fake_server);
}

}  // namespace passwords_helper

PasswordSyncActiveChecker::PasswordSyncActiveChecker(
    syncer::SyncServiceImpl* service)
    : SingleClientStatusChangeChecker(service) {}

PasswordSyncActiveChecker::~PasswordSyncActiveChecker() = default;

bool PasswordSyncActiveChecker::IsExitConditionSatisfied(std::ostream* os) {
  return service()->GetActiveDataTypes().Has(syncer::PASSWORDS);
}

PasswordSyncInactiveChecker::PasswordSyncInactiveChecker(
    syncer::SyncServiceImpl* service)
    : SingleClientStatusChangeChecker(service) {}

PasswordSyncInactiveChecker::~PasswordSyncInactiveChecker() = default;

bool PasswordSyncInactiveChecker::IsExitConditionSatisfied(std::ostream* os) {
  return !service()->GetActiveDataTypes().Has(syncer::PASSWORDS);
}

SamePasswordFormsChecker::SamePasswordFormsChecker(PasswordForm::Store store)
    : MultiClientStatusChangeChecker(
          sync_datatype_helper::test()->GetSyncServices()),
      store_(store) {}

SamePasswordFormsChecker::~SamePasswordFormsChecker() = default;

// This method needs protection against re-entrancy.
//
// This function indirectly calls GetLogins(), which starts a RunLoop on the UI
// thread.  This can be a problem, since the next task to execute could very
// well contain a SyncServiceObserver::OnStateChanged() event, which would
// trigger another call to this here function, and start another layer of
// nested RunLoops.  That makes the StatusChangeChecker's Quit() method
// ineffective.
//
// The work-around is to not allow re-entrancy.  But we can't just drop
// IsExitConditionSatisifed() calls if one is already in progress.  Instead, we
// set a flag to ask the current execution of IsExitConditionSatisfied() to be
// re-run.  This ensures that the return value is always based on the most
// up-to-date state.
bool SamePasswordFormsChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for matching passwords";

  if (in_progress_) {
    LOG(WARNING) << "Setting flag and returning early to prevent nesting.";
    needs_recheck_ = true;
    return false;
  }

  // Keep retrying until we get a good reading.
  bool result = false;
  in_progress_ = true;
  do {
    needs_recheck_ = false;
    result = passwords_helper::AllProfilesContainSamePasswordForms(store_);
  } while (needs_recheck_);
  in_progress_ = false;
  return result;
}

SamePasswordFormsAsVerifierChecker::SamePasswordFormsAsVerifierChecker(int i)
    : SingleClientStatusChangeChecker(
          sync_datatype_helper::test()->GetSyncService(i)),
      index_(i) {}

// This method uses the same re-entrancy prevention trick as
// the SamePasswordFormsChecker.
bool SamePasswordFormsAsVerifierChecker::IsExitConditionSatisfied(
    std::ostream* os) {
  *os << "Waiting for passwords to match verifier";

  if (in_progress_) {
    LOG(WARNING) << "Setting flag and returning early to prevent nesting.";
    needs_recheck_ = true;
    return false;
  }

  // Keep retrying until we get a good reading.
  bool result = false;
  in_progress_ = true;
  do {
    needs_recheck_ = false;
    result =
        passwords_helper::ProfileContainsSamePasswordFormsAsVerifier(index_);
  } while (needs_recheck_);
  in_progress_ = false;
  return result;
}

PasswordFormsChecker::PasswordFormsChecker(
    int index,
    const std::vector<password_manager::PasswordForm>& expected_forms)
    : SingleClientStatusChangeChecker(
          sync_datatype_helper::test()->GetSyncService(index)),
      index_(index) {
  for (const password_manager::PasswordForm& password_form : expected_forms) {
    expected_forms_.push_back(
        std::make_unique<password_manager::PasswordForm>(password_form));
  }
}

PasswordFormsChecker::~PasswordFormsChecker() = default;

// This method uses the same re-entrancy prevention trick as
// the SamePasswordFormsChecker.
bool PasswordFormsChecker::IsExitConditionSatisfied(std::ostream* os) {
  if (in_progress_) {
    LOG(WARNING) << "Setting flag and returning early to prevent nesting.";
    *os << "Setting flag and returning early to prevent nesting.";
    needs_recheck_ = true;
    return false;
  }

  // Keep retrying until we get a good reading.
  bool result = false;
  in_progress_ = true;
  do {
    needs_recheck_ = false;
    result = IsExitConditionSatisfiedImpl(os);
  } while (needs_recheck_);
  in_progress_ = false;
  return result;
}

bool PasswordFormsChecker::IsExitConditionSatisfiedImpl(std::ostream* os) {
  std::vector<std::unique_ptr<PasswordForm>> forms =
      passwords_helper::GetLogins(
          passwords_helper::GetProfilePasswordStoreInterface(index_));

  std::ostringstream mismatch_details_stream;
  bool is_matching = password_manager::ContainsEqualPasswordFormsUnordered(
      expected_forms_, forms, &mismatch_details_stream);
  if (!is_matching) {
    *os << "Profile " << index_
        << " does not contain the same Password forms as expected. "
        << mismatch_details_stream.str();
  }
  return is_matching;
}

ServerPasswordsEqualityChecker::ServerPasswordsEqualityChecker(
    const std::vector<password_manager::PasswordForm>& expected_forms,
    const std::string& encryption_passphrase,
    const syncer::KeyDerivationParams& key_derivation_params)
    : cryptographer_(syncer::CryptographerImpl::FromSingleKeyForTesting(
          encryption_passphrase,
          key_derivation_params)) {
  for (const password_manager::PasswordForm& password_form : expected_forms) {
    expected_forms_.push_back(
        std::make_unique<password_manager::PasswordForm>(password_form));
    // |in_store| field is specific for the clients, clean it up, since server
    // specifics don't have it.
    expected_forms_.back()->in_store =
        password_manager::PasswordForm::Store::kNotSet;
  }
}

ServerPasswordsEqualityChecker::~ServerPasswordsEqualityChecker() = default;

bool ServerPasswordsEqualityChecker::IsExitConditionSatisfied(
    std::ostream* os) {
  *os << "Waiting for server passwords to match the expected value.";

  std::vector<sync_pb::SyncEntity> entities =
      fake_server()->GetSyncEntitiesByDataType(syncer::PASSWORDS);
  if (expected_forms_.size() != entities.size()) {
    *os << "Server doesn't not contain same amount of passwords ("
        << entities.size() << ") as expected (" << expected_forms_.size()
        << ").";
    return false;
  }

  std::vector<std::unique_ptr<password_manager::PasswordForm>>
      server_password_forms;
  for (const auto& entity : entities) {
    if (!entity.specifics().has_password()) {
      *os << "Server stores corrupted password.";
      return false;
    }

    sync_pb::PasswordSpecificsData decrypted;
    if (!cryptographer_->Decrypt(entity.specifics().password().encrypted(),
                                 &decrypted)) {
      *os << "Can't decrypt server password.";
      return false;
    }
    server_password_forms.push_back(
        std::make_unique<password_manager::PasswordForm>(
            password_manager::PasswordFromSpecifics(decrypted)));
  }

  std::ostringstream mismatch_details_stream;
  bool is_matching = password_manager::ContainsEqualPasswordFormsUnordered(
      expected_forms_, server_password_forms, &mismatch_details_stream);
  if (!is_matching) {
    *os << "Server does not contain the same Password forms as expected. "
        << mismatch_details_stream.str();
  }
  return is_matching;
}

PasswordFormsAddedChecker::PasswordFormsAddedChecker(
    password_manager::PasswordStoreInterface* password_store,
    size_t expected_new_password_forms)
    : password_store_(password_store),
      expected_new_password_forms_(expected_new_password_forms) {
  password_store_->AddObserver(this);
}

PasswordFormsAddedChecker::~PasswordFormsAddedChecker() {
  password_store_->RemoveObserver(this);
}

bool PasswordFormsAddedChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for " << expected_new_password_forms_
      << " passwords added to the store. ";

  *os << "Current number of added password forms to the store: "
      << num_added_passwords_;
  return num_added_passwords_ == expected_new_password_forms_;
}

void PasswordFormsAddedChecker::OnLoginsChanged(
    password_manager::PasswordStoreInterface* store,
    const password_manager::PasswordStoreChangeList& changes) {
  for (const password_manager::PasswordStoreChange& change : changes) {
    if (change.type() == password_manager::PasswordStoreChange::ADD) {
      num_added_passwords_++;
    }
  }

  CheckExitCondition();
}

void PasswordFormsAddedChecker::OnLoginsRetained(
    password_manager::PasswordStoreInterface* store,
    const std::vector<password_manager::PasswordForm>& retained_passwords) {
  // Not used.
}
