// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/passwords_helper.h"

#include <sstream>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "components/password_manager/core/browser/insecure_credentials_consumer.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/sync/engine/loopback_server/persistent_unique_client_entity.h"
#include "components/sync/nigori/cryptographer_impl.h"
#include "content/public/test/test_utils.h"
#include "net/base/escape.h"
#include "url/gurl.h"

using password_manager::InsecureCredential;
using password_manager::PasswordForm;
using password_manager::PasswordStore;
using sync_datatype_helper::test;

namespace {

const char kFakeSignonRealm[] = "http://fake-signon-realm.google.com/";
const char kIndexedFakeOrigin[] = "http://fake-signon-realm.google.com/%d";

// We use a WaitableEvent to wait when logins are added, removed, or updated
// instead of running the UI message loop because of a restriction that
// prevents a DB thread from initiating a quit of the UI message loop.
void PasswordStoreCallback(base::WaitableEvent* wait_event) {
  // Wake up passwords_helper::AddLogin.
  wait_event->Signal();
}

class PasswordStoreConsumerHelper
    : public password_manager::PasswordStoreConsumer {
 public:
  PasswordStoreConsumerHelper() {}

  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>> results) override {
    result_.swap(results);
    run_loop_.Quit();
  }

  std::vector<std::unique_ptr<PasswordForm>> WaitForResult() {
    DCHECK(!run_loop_.running());
    content::RunThisRunLoop(&run_loop_);
    return std::move(result_);
  }

 private:
  base::RunLoop run_loop_;
  std::vector<std::unique_ptr<PasswordForm>> result_;

  DISALLOW_COPY_AND_ASSIGN(PasswordStoreConsumerHelper);
};

class InsecureCredentialsConsumerHelper
    : public password_manager::InsecureCredentialsConsumer {
 public:
  InsecureCredentialsConsumerHelper() = default;

  void OnGetInsecureCredentials(
      std::vector<InsecureCredential> insecure_credentials) override {
    insecure_credentials_ = std::move(insecure_credentials);
    run_loop_.Quit();
  }

  std::vector<InsecureCredential> WaitForResult() {
    DCHECK(!run_loop_.running());
    content::RunThisRunLoop(&run_loop_);
    return insecure_credentials_;
  }

 private:
  base::RunLoop run_loop_;
  std::vector<InsecureCredential> insecure_credentials_;

  DISALLOW_COPY_AND_ASSIGN(InsecureCredentialsConsumerHelper);
};

// PasswordForm::date_synced is a local field. Therefore it may be different
// across clients.
void ClearSyncDateField(std::vector<std::unique_ptr<PasswordForm>>* forms) {
  for (auto& form : *forms) {
    form->date_synced = base::Time();
  }
}

sync_pb::PasswordSpecificsData SpecificsDataFromPasswordForm(
    const password_manager::PasswordForm& password_form) {
  sync_pb::PasswordSpecificsData password_data;
  password_data.set_scheme(static_cast<int>(password_form.scheme));
  password_data.set_signon_realm(password_form.signon_realm);
  password_data.set_origin(password_form.url.spec());
  password_data.set_action(password_form.action.spec());
  password_data.set_username_element(
      base::UTF16ToUTF8(password_form.username_element));
  password_data.set_password_element(
      base::UTF16ToUTF8(password_form.password_element));
  password_data.set_username_value(
      base::UTF16ToUTF8(password_form.username_value));
  password_data.set_password_value(
      base::UTF16ToUTF8(password_form.password_value));
  password_data.set_date_last_used(
      password_form.date_last_used.ToDeltaSinceWindowsEpoch().InMicroseconds());
  password_data.set_date_created(
      password_form.date_created.ToDeltaSinceWindowsEpoch().InMicroseconds());
  password_data.set_blacklisted(password_form.blocked_by_user);
  password_data.set_type(static_cast<int>(password_form.type));
  password_data.set_times_used(password_form.times_used);
  password_data.set_display_name(base::UTF16ToUTF8(password_form.display_name));
  password_data.set_avatar_url(password_form.icon_url.spec());
  password_data.set_federation_url(
      password_form.federation_origin.opaque()
          ? std::string()
          : password_form.federation_origin.Serialize());
  return password_data;
}

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
  return net::EscapePath(GURL(password_data.origin()).spec()) + "|" +
         net::EscapePath(password_data.username_element()) + "|" +
         net::EscapePath(password_data.username_value()) + "|" +
         net::EscapePath(password_data.password_element()) + "|" +
         net::EscapePath(password_data.signon_realm());
}

}  // namespace

namespace passwords_helper {

void AddLogin(PasswordStore* store, const PasswordForm& form) {
  ASSERT_TRUE(store);
  base::WaitableEvent wait_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  store->AddLogin(form);
  store->ScheduleTask(base::BindOnce(&PasswordStoreCallback, &wait_event));
  wait_event.Wait();
}

void AddInsecureCredential(PasswordStore* store,
                           const InsecureCredential& issue) {
  ASSERT_TRUE(store);
  base::WaitableEvent wait_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  store->AddInsecureCredential(issue);
  store->ScheduleTask(base::BindOnce(&PasswordStoreCallback, &wait_event));
  wait_event.Wait();
}

void UpdateLogin(PasswordStore* store, const PasswordForm& form) {
  ASSERT_TRUE(store);
  base::WaitableEvent wait_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  store->UpdateLogin(form);
  store->ScheduleTask(base::BindOnce(&PasswordStoreCallback, &wait_event));
  wait_event.Wait();
}

void UpdateLoginWithPrimaryKey(PasswordStore* store,
                               const PasswordForm& new_form,
                               const PasswordForm& old_form) {
  ASSERT_TRUE(store);
  base::WaitableEvent wait_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  store->UpdateLoginWithPrimaryKey(new_form, old_form);
  store->ScheduleTask(base::BindOnce(&PasswordStoreCallback, &wait_event));
  wait_event.Wait();
}

std::vector<std::unique_ptr<PasswordForm>> GetLogins(PasswordStore* store) {
  EXPECT_TRUE(store);
  password_manager::PasswordStore::FormDigest matcher_form = {
      PasswordForm::Scheme::kHtml, kFakeSignonRealm, GURL()};
  PasswordStoreConsumerHelper consumer;
  store->GetLogins(matcher_form, &consumer);
  return consumer.WaitForResult();
}

std::vector<std::unique_ptr<PasswordForm>> GetAllLogins(PasswordStore* store) {
  EXPECT_TRUE(store);
  PasswordStoreConsumerHelper consumer;
  store->GetAllLogins(&consumer);
  return consumer.WaitForResult();
}

std::vector<InsecureCredential> GetAllInsecureCredentials(
    PasswordStore* store) {
  DCHECK(store);
  InsecureCredentialsConsumerHelper consumer;
  store->GetAllInsecureCredentials(&consumer);
  return consumer.WaitForResult();
}

void RemoveLogin(PasswordStore* store, const PasswordForm& form) {
  ASSERT_TRUE(store);
  base::WaitableEvent wait_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  store->RemoveLogin(form);
  store->ScheduleTask(base::BindOnce(&PasswordStoreCallback, &wait_event));
  wait_event.Wait();
}

void RemoveLogins(PasswordStore* store) {
  std::vector<std::unique_ptr<PasswordForm>> forms = GetLogins(store);
  for (const auto& form : forms) {
    RemoveLogin(store, *form);
  }
}

void RemoveInsecureCredentials(PasswordStore* store,
                               const InsecureCredential& credential) {
  ASSERT_TRUE(store);
  base::WaitableEvent wait_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  store->RemoveInsecureCredentials(
      credential.signon_realm, credential.username,
      // kRemove used for arbitrary reason just for test.
      password_manager::RemoveInsecureCredentialsReason::kRemove);
  store->ScheduleTask(base::BindOnce(&PasswordStoreCallback, &wait_event));
  wait_event.Wait();
}

PasswordStore* GetPasswordStore(int index) {
  return PasswordStoreFactory::GetForProfile(test()->GetProfile(index),
                                             ServiceAccessType::IMPLICIT_ACCESS)
      .get();
}

PasswordStore* GetVerifierPasswordStore() {
  return PasswordStoreFactory::GetForProfile(test()->verifier(),
                                             ServiceAccessType::IMPLICIT_ACCESS)
      .get();
}

PasswordStore* GetAccountPasswordStore(int index) {
  return AccountPasswordStoreFactory::GetForProfile(
             test()->GetProfile(index), ServiceAccessType::IMPLICIT_ACCESS)
      .get();
}

bool ProfileContainsSamePasswordFormsAsVerifier(int index) {
  std::vector<std::unique_ptr<PasswordForm>> verifier_forms =
      GetLogins(GetVerifierPasswordStore());
  std::vector<std::unique_ptr<PasswordForm>> forms =
      GetLogins(GetPasswordStore(index));
  ClearSyncDateField(&forms);

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

bool ProfilesContainSamePasswordForms(int index_a, int index_b) {
  std::vector<std::unique_ptr<PasswordForm>> forms_a =
      GetLogins(GetPasswordStore(index_a));
  std::vector<std::unique_ptr<PasswordForm>> forms_b =
      GetLogins(GetPasswordStore(index_b));
  ClearSyncDateField(&forms_a);
  ClearSyncDateField(&forms_b);

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

bool AllProfilesContainSamePasswordForms() {
  for (int i = 1; i < test()->num_clients(); ++i) {
    if (!ProfilesContainSamePasswordForms(0, i)) {
      DVLOG(1) << "Profile " << i
               << " does not contain the same password"
                  " forms as Profile 0.";
      return false;
    }
  }
  return true;
}

bool AllProfilesContainSameInsecurePasswords() {
  auto MatchesProfile0 = testing::Matches(testing::UnorderedElementsAreArray(
      GetAllInsecureCredentials(GetPasswordStore(0))));
  for (int i = 1; i < test()->num_clients(); ++i) {
    if (!MatchesProfile0(GetAllInsecureCredentials(GetPasswordStore(i)))) {
      DVLOG(1) << "Profile " << i
               << " does not contain the same insecure passwords as Profile 0.";
      return false;
    }
  }
  return true;
}

int GetPasswordCount(int index) {
  return GetLogins(GetPasswordStore(index)).size();
}

int GetVerifierPasswordCount() {
  return GetLogins(GetVerifierPasswordStore()).size();
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

InsecureCredential CreateInsecureCredential(
    int index,
    password_manager::InsecureType type) {
  InsecureCredential issue;
  issue.signon_realm = kFakeSignonRealm;
  // This should stay compatible with the implementation of
  // CreateTestPasswordForm() and use the same username format.
  issue.username = base::ASCIIToUTF16(base::StringPrintf("username%d", index));
  issue.create_time = base::Time::Now();
  issue.insecure_type = type;
  return issue;
}

void InjectEncryptedServerPassword(
    const password_manager::PasswordForm& form,
    const std::string& encryption_passphrase,
    const syncer::KeyDerivationParams& key_derivation_params,
    fake_server::FakeServer* fake_server) {
  InjectEncryptedServerPassword(SpecificsDataFromPasswordForm(form),
                                encryption_passphrase, key_derivation_params,
                                fake_server);
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
  InjectKeystoreEncryptedServerPassword(SpecificsDataFromPasswordForm(form),
                                        fake_server);
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
    syncer::ProfileSyncService* service)
    : SingleClientStatusChangeChecker(service) {}
PasswordSyncActiveChecker::~PasswordSyncActiveChecker() = default;

bool PasswordSyncActiveChecker::IsExitConditionSatisfied(std::ostream* os) {
  return service()->GetActiveDataTypes().Has(syncer::PASSWORDS);
}

SamePasswordFormsChecker::SamePasswordFormsChecker()
    : SamePasswordFormsChecker(CheckForInsecure(false)) {}

SamePasswordFormsChecker::SamePasswordFormsChecker(
    CheckForInsecure check_for_insecure)
    : MultiClientStatusChangeChecker(
          sync_datatype_helper::test()->GetSyncServices()),
      check_for_insecure_(check_for_insecure) {}

SamePasswordFormsChecker::~SamePasswordFormsChecker() = default;

// This method needs protection against re-entrancy.
//
// This function indirectly calls GetLogins(), which starts a RunLoop on the UI
// thread.  This can be a problem, since the next task to execute could very
// well contain a ProfileSyncService::OnStateChanged() event, which would
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
    result = passwords_helper::AllProfilesContainSamePasswordForms() &&
             (!check_for_insecure_ ||
              passwords_helper::AllProfilesContainSameInsecurePasswords());
  } while (needs_recheck_);
  in_progress_ = false;
  return result;
}

SamePasswordFormsAsVerifierChecker::SamePasswordFormsAsVerifierChecker(int i)
    : SingleClientStatusChangeChecker(
          sync_datatype_helper::test()->GetSyncService(i)),
      index_(i),
      in_progress_(false),
      needs_recheck_(false) {}

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
      index_(index),
      in_progress_(false),
      needs_recheck_(false) {
  for (auto& password_form : expected_forms) {
    expected_forms_.push_back(
        std::make_unique<password_manager::PasswordForm>(password_form));
  }
  ClearSyncDateField(&expected_forms_);
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
      passwords_helper::GetLogins(passwords_helper::GetPasswordStore(index_));
  ClearSyncDateField(&forms);

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
