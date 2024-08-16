// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/feature_list.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/test/integration/committed_all_nudged_changes_checker.h"
#include "chrome/browser/sync/test/integration/encryption_helper.h"
#include "chrome/browser/sync/test/integration/passwords_helper.h"
#include "chrome/browser/sync/test/integration/secondary_account_helper.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/features/password_manager_features_util.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/sync/password_sync_bridge.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/data_type.h"
#include "components/sync/engine/cycle/entity_change_metric_recording.h"
#include "components/sync/nigori/cryptographer_impl.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/sync/test/fake_server_nigori_helper.h"
#include "components/sync/test/test_matchers.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/features.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"

namespace {

using password_manager::features_util::OptInToAccountStorage;
using passwords_helper::CreateTestPasswordForm;
using passwords_helper::GetPasswordCount;
using passwords_helper::GetProfilePasswordStoreInterface;
using passwords_helper::GetVerifierPasswordCount;
using passwords_helper::GetVerifierProfilePasswordStoreInterface;
using passwords_helper::ProfileContainsSamePasswordFormsAsVerifier;

using password_manager::PasswordForm;

using testing::Contains;
using testing::ElementsAre;
using testing::Field;
using testing::SizeIs;

#if !BUILDFLAG(IS_CHROMEOS_ASH)
const syncer::SyncFirstSetupCompleteSource kSetSourceFromTest =
    syncer::SyncFirstSetupCompleteSource::BASIC_FLOW;
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

MATCHER_P3(HasPasswordValueAndUnsupportedFields,
           cryptographer,
           password_value,
           unknown_fields,
           "") {
  sync_pb::PasswordSpecificsData decrypted;
  cryptographer->Decrypt(arg.specifics().password().encrypted(), &decrypted);
  return decrypted.password_value() == password_value &&
         decrypted.unknown_fields() == unknown_fields;
}

std::string CreateSerializedProtoField(int field_number,
                                       const std::string& value) {
  std::string result;
  google::protobuf::io::StringOutputStream string_stream(&result);
  google::protobuf::io::CodedOutputStream output(&string_stream);
  google::protobuf::internal::WireFormatLite::WriteTag(
      field_number,
      google::protobuf::internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED,
      &output);
  output.WriteVarint32(value.size());
  output.WriteString(value);
  return result;
}

class SingleClientPasswordsSyncTest : public SyncTest {
 public:
  SingleClientPasswordsSyncTest() : SyncTest(SINGLE_CLIENT) {}
  ~SingleClientPasswordsSyncTest() override = default;
};

class SingleClientPasswordsSyncTestWithVerifier
    : public SingleClientPasswordsSyncTest {
 public:
  SingleClientPasswordsSyncTestWithVerifier() = default;
  ~SingleClientPasswordsSyncTestWithVerifier() override = default;

  bool UseVerifier() override {
    // TODO(crbug.com/40152785): rewrite tests to not use verifier.
    return true;
  }
};

IN_PROC_BROWSER_TEST_F(SingleClientPasswordsSyncTestWithVerifier, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  PasswordForm form = CreateTestPasswordForm(0);
  GetVerifierProfilePasswordStoreInterface()->AddLogin(form);
  ASSERT_EQ(1, GetVerifierPasswordCount());
  GetProfilePasswordStoreInterface(0)->AddLogin(form);
  ASSERT_EQ(1, GetPasswordCount(0));

  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(ProfileContainsSamePasswordFormsAsVerifier(0));
  ASSERT_EQ(1, GetPasswordCount(0));
}

// Verifies that committed passwords contain the appropriate proto fields, and
// in particular lack some others that could potentially contain unencrypted
// data. In this test, custom passphrase is NOT set.
IN_PROC_BROWSER_TEST_F(SingleClientPasswordsSyncTestWithVerifier,
                       CommitWithoutCustomPassphrase) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  PasswordForm form = CreateTestPasswordForm(0);
  GetVerifierProfilePasswordStoreInterface()->AddLogin(form);
  ASSERT_EQ(1, GetVerifierPasswordCount());
  GetProfilePasswordStoreInterface(0)->AddLogin(form);
  ASSERT_EQ(1, GetPasswordCount(0));
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  const std::vector<sync_pb::SyncEntity> entities =
      fake_server_->GetSyncEntitiesByDataType(syncer::PASSWORDS);
  ASSERT_EQ(1U, entities.size());
  EXPECT_EQ("", entities[0].non_unique_name());
  EXPECT_TRUE(entities[0].specifics().password().has_encrypted());
  EXPECT_FALSE(
      entities[0].specifics().password().has_client_only_encrypted_data());
  EXPECT_TRUE(entities[0].specifics().password().has_unencrypted_metadata());
  EXPECT_TRUE(
      entities[0].specifics().password().unencrypted_metadata().has_url());
  EXPECT_TRUE(entities[0]
                  .specifics()
                  .password()
                  .unencrypted_metadata()
                  .has_password_issues());
}

// Same as above but with custom passphrase set, which requires to prune commit
// data even further.
IN_PROC_BROWSER_TEST_F(SingleClientPasswordsSyncTestWithVerifier,
                       CommitWithCustomPassphrase) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  GetSyncService(0)->GetUserSettings()->SetEncryptionPassphrase("hunter2");

  PasswordForm form = CreateTestPasswordForm(0);
  GetVerifierProfilePasswordStoreInterface()->AddLogin(form);
  ASSERT_EQ(1, GetVerifierPasswordCount());
  GetProfilePasswordStoreInterface(0)->AddLogin(form);
  ASSERT_EQ(1, GetPasswordCount(0));
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  const std::vector<sync_pb::SyncEntity> entities =
      fake_server_->GetSyncEntitiesByDataType(syncer::PASSWORDS);
  ASSERT_EQ(1U, entities.size());
  EXPECT_EQ("", entities[0].non_unique_name());
  EXPECT_TRUE(entities[0].specifics().password().has_encrypted());
  EXPECT_FALSE(
      entities[0].specifics().password().has_client_only_encrypted_data());
  EXPECT_FALSE(entities[0].specifics().password().has_unencrypted_metadata());
}

// Tests the scenario when a syncing user enables a custom passphrase. PASSWORDS
// should be recommitted with the new encryption key.
IN_PROC_BROWSER_TEST_F(SingleClientPasswordsSyncTestWithVerifier,
                       ReencryptsDataWhenPassphraseIsSet) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(
      ServerPassphraseTypeChecker(syncer::PassphraseType::kKeystorePassphrase)
          .Wait());

  PasswordForm form = CreateTestPasswordForm(0);
  GetVerifierProfilePasswordStoreInterface()->AddLogin(form);
  ASSERT_EQ(1, GetVerifierPasswordCount());
  GetProfilePasswordStoreInterface(0)->AddLogin(form);
  ASSERT_EQ(1, GetPasswordCount(0));
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  std::string prior_encryption_key_name;
  {
    const std::vector<sync_pb::SyncEntity> entities =
        fake_server_->GetSyncEntitiesByDataType(syncer::PASSWORDS);
    ASSERT_EQ(1U, entities.size());
    ASSERT_EQ("", entities[0].non_unique_name());
    ASSERT_TRUE(entities[0].specifics().password().has_encrypted());
    ASSERT_FALSE(
        entities[0].specifics().password().has_client_only_encrypted_data());
    ASSERT_TRUE(entities[0].specifics().password().has_unencrypted_metadata());
    prior_encryption_key_name =
        entities[0].specifics().password().encrypted().key_name();
  }

  ASSERT_FALSE(prior_encryption_key_name.empty());

  GetSyncService(0)->GetUserSettings()->SetEncryptionPassphrase("hunter2");
  ASSERT_TRUE(
      ServerPassphraseTypeChecker(syncer::PassphraseType::kCustomPassphrase)
          .Wait());
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  const std::vector<sync_pb::SyncEntity> entities =
      fake_server_->GetSyncEntitiesByDataType(syncer::PASSWORDS);
  ASSERT_EQ(1U, entities.size());
  EXPECT_EQ("", entities[0].non_unique_name());
  EXPECT_TRUE(entities[0].specifics().password().has_encrypted());
  EXPECT_FALSE(
      entities[0].specifics().password().has_client_only_encrypted_data());
  EXPECT_FALSE(entities[0].specifics().password().has_unencrypted_metadata());

  const std::string new_encryption_key_name =
      entities[0].specifics().password().encrypted().key_name();
  EXPECT_FALSE(new_encryption_key_name.empty());
  EXPECT_NE(new_encryption_key_name, prior_encryption_key_name);
}

IN_PROC_BROWSER_TEST_F(SingleClientPasswordsSyncTest,
                       PRE_PersistProgressMarkerOnRestart) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  PasswordForm form = CreateTestPasswordForm(0);
  GetProfilePasswordStoreInterface(0)->AddLogin(form);
  ASSERT_EQ(1, GetPasswordCount(0));
  // Setup sync, wait for its completion, and make sure changes were synced.
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  // Upon a local creation, the received update will be seen as reflection and
  // get counted as incremental update.
  EXPECT_EQ(1, histogram_tester.GetBucketCount(
                   "Sync.DataTypeEntityChange.PASSWORD",
                   syncer::DataTypeEntityChange::kRemoteNonInitialUpdate));
}

IN_PROC_BROWSER_TEST_F(SingleClientPasswordsSyncTest,
                       PersistProgressMarkerOnRestart) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(SetupClients());
  ASSERT_EQ(1, GetPasswordCount(0));

  // Wait for data types to be ready for sync and trigger a sync cycle.
  // Otherwise, TriggerRefresh() would be no-op.
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion());
  GetSyncService(0)->TriggerRefresh({syncer::PASSWORDS});

  // After restart, the last sync cycle snapshot should be empty. Once a sync
  // request happened (e.g. by a poll), that snapshot is populated. We use the
  // following checker to simply wait for an non-empty snapshot.
  EXPECT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  // If that metadata hasn't been properly persisted, the password stored on the
  // server will be received at the client as an initial update or an
  // incremental once.
  EXPECT_EQ(0, histogram_tester.GetBucketCount(
                   "Sync.DataTypeEntityChange.PASSWORD",
                   syncer::DataTypeEntityChange::kRemoteInitialUpdate));
  EXPECT_EQ(0, histogram_tester.GetBucketCount(
                   "Sync.DataTypeEntityChange.PASSWORD",
                   syncer::DataTypeEntityChange::kRemoteNonInitialUpdate));
}

class SingleClientPasswordsWithAccountStorageSyncTest : public SyncTest {
 public:
  SingleClientPasswordsWithAccountStorageSyncTest() : SyncTest(SINGLE_CLIENT) {}

  SingleClientPasswordsWithAccountStorageSyncTest(
      const SingleClientPasswordsWithAccountStorageSyncTest&) = delete;
  SingleClientPasswordsWithAccountStorageSyncTest& operator=(
      const SingleClientPasswordsWithAccountStorageSyncTest&) = delete;

  ~SingleClientPasswordsWithAccountStorageSyncTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    test_signin_client_subscription_ =
        secondary_account_helper::SetUpSigninClient(&test_url_loader_factory_);
    SyncTest::SetUpInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    secondary_account_helper::InitNetwork();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    SyncTest::SetUpOnMainThread();

    fake_server::SetKeystoreNigoriInFakeServer(GetFakeServer());
  }

  void AddTestPasswordToFakeServer() {
    sync_pb::PasswordSpecificsData password_data;
    // Used for computing the client tag.
    password_data.set_origin("https://origin.com");
    password_data.set_username_element("username_element");
    password_data.set_username_value("username_value");
    password_data.set_password_element("password_element");
    password_data.set_signon_realm("abc");
    // Other data.
    password_data.set_password_value("password_value");

    passwords_helper::InjectKeystoreEncryptedServerPassword(password_data,
                                                            GetFakeServer());
  }

 private:
  base::test::ScopedFeatureList feature_list_;

  base::CallbackListSubscription test_signin_client_subscription_;
};

// Sanity check: For Sync-the-feature, password data still ends up in the
// profile database.
IN_PROC_BROWSER_TEST_F(SingleClientPasswordsWithAccountStorageSyncTest,
                       StoresDataForSyncingPrimaryAccountInProfileDB) {
  AddTestPasswordToFakeServer();

  // Sign in and enable Sync.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PASSWORDS));

  // Make sure the password showed up in the profile store and not in the
  // account store.
  password_manager::PasswordStoreInterface* profile_store =
      passwords_helper::GetProfilePasswordStoreInterface(0);
  EXPECT_EQ(passwords_helper::GetAllLogins(profile_store).size(), 1u);

  password_manager::PasswordStoreInterface* account_store =
      passwords_helper::GetAccountPasswordStoreInterface(0);
  EXPECT_EQ(passwords_helper::GetAllLogins(account_store).size(), 0u);
}

// On ChromeOS, Sync-the-feature gets started automatically once a primary
// account is signed in and the transport mode is not a thing.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(SingleClientPasswordsWithAccountStorageSyncTest,
                       StoresDataForNonSyncingPrimaryAccountInAccountDB) {
  AddTestPasswordToFakeServer();

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Setup a primary account, but don't actually enable Sync-the-feature (so
  // that Sync will start in transport mode).
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());

  // Let the user opt in to the account-scoped password storage, and wait for it
  // to become active.
  OptInToAccountStorage(GetProfile(0)->GetPrefs(), GetSyncService(0));
  PasswordSyncActiveChecker(GetSyncService(0)).Wait();
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PASSWORDS));

  // Make sure the password showed up in the account store and not in the
  // profile store.
  password_manager::PasswordStoreInterface* profile_store =
      passwords_helper::GetProfilePasswordStoreInterface(0);
  EXPECT_EQ(passwords_helper::GetAllLogins(profile_store).size(), 0u);

  password_manager::PasswordStoreInterface* account_store =
      passwords_helper::GetAccountPasswordStoreInterface(0);
  EXPECT_EQ(passwords_helper::GetAllLogins(account_store).size(), 1u);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// The unconsented primary account isn't supported on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(SingleClientPasswordsWithAccountStorageSyncTest,
                       StoresDataForSecondaryAccountInAccountDB) {
  AddTestPasswordToFakeServer();

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Setup Sync without consent (i.e. in transport mode).
  secondary_account_helper::SignInUnconsentedAccount(
      GetProfile(0), &test_url_loader_factory_, "user@email.com");
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());

  // Let the user opt in to the account-scoped password storage, and wait for it
  // to become active.
  OptInToAccountStorage(GetProfile(0)->GetPrefs(), GetSyncService(0));
  PasswordSyncActiveChecker(GetSyncService(0)).Wait();
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PASSWORDS));

  // Make sure the password showed up in the account store and not in the
  // profile store.
  password_manager::PasswordStoreInterface* profile_store =
      passwords_helper::GetProfilePasswordStoreInterface(0);
  EXPECT_EQ(passwords_helper::GetAllLogins(profile_store).size(), 0u);

  password_manager::PasswordStoreInterface* account_store =
      passwords_helper::GetAccountPasswordStoreInterface(0);
  EXPECT_EQ(passwords_helper::GetAllLogins(account_store).size(), 1u);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// ChromeOS does not support signing out of a primary account.
#if !BUILDFLAG(IS_CHROMEOS_ASH)

// Sanity check: The profile database should *not* get cleared on signout.
IN_PROC_BROWSER_TEST_F(SingleClientPasswordsWithAccountStorageSyncTest,
                       DoesNotClearProfileDBOnSignout) {
  AddTestPasswordToFakeServer();

  // Sign in and enable Sync.
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureEnabled());

  // Make sure the password showed up in the profile store.
  password_manager::PasswordStoreInterface* profile_store =
      passwords_helper::GetProfilePasswordStoreInterface(0);
  ASSERT_EQ(passwords_helper::GetAllLogins(profile_store).size(), 1u);

  // Sign out again.
  GetClient(0)->SignOutPrimaryAccount();
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());

  // Make sure the password is still in the store.
  ASSERT_EQ(passwords_helper::GetAllLogins(profile_store).size(), 1u);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// The unconsented primary account isn't supported on ChromeOS so Sync won't
// start up for an unconsented account.
// Signing out on Lacros is not possible.
#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SingleClientPasswordsWithAccountStorageSyncTest,
                       ClearsAccountDBOnSignout) {
  AddTestPasswordToFakeServer();

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Setup Sync without consent (i.e. in transport mode).
  secondary_account_helper::SignInUnconsentedAccount(
      GetProfile(0), &test_url_loader_factory_, "user@email.com");
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());

  // Let the user opt in to the account-scoped password storage, and wait for it
  // to become active.
  OptInToAccountStorage(GetProfile(0)->GetPrefs(), GetSyncService(0));
  PasswordSyncActiveChecker(GetSyncService(0)).Wait();

  // Make sure the password showed up in the account store.
  password_manager::PasswordStoreInterface* account_store =
      passwords_helper::GetAccountPasswordStoreInterface(0);
  ASSERT_EQ(passwords_helper::GetAllLogins(account_store).size(), 1u);

  // Sign out again.
  secondary_account_helper::SignOut(GetProfile(0), &test_url_loader_factory_);

  // Make sure the password is gone from the store.
  ASSERT_EQ(passwords_helper::GetAllLogins(account_store).size(), 0u);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

// The unconsented primary account isn't supported on ChromeOS so Sync won't
// start up for an unconsented account.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(SingleClientPasswordsWithAccountStorageSyncTest,
                       SwitchesStoresOnMakingAccountPrimary) {
  AddTestPasswordToFakeServer();

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Setup Sync for an unconsented account (i.e. in transport mode).
  AccountInfo account_info = secondary_account_helper::SignInUnconsentedAccount(
      GetProfile(0), &test_url_loader_factory_, "user@email.com");
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());

  // Let the user opt in to the account-scoped password storage, and wait for it
  // to become active.
  OptInToAccountStorage(GetProfile(0)->GetPrefs(), GetSyncService(0));
  PasswordSyncActiveChecker(GetSyncService(0)).Wait();

  // Make sure the password showed up in the account store.
  password_manager::PasswordStoreInterface* account_store =
      passwords_helper::GetAccountPasswordStoreInterface(0);
  ASSERT_EQ(passwords_helper::GetAllLogins(account_store).size(), 1u);

  // Turn on Sync-the-feature.
  secondary_account_helper::GrantSyncConsent(GetProfile(0), "user@email.com");
  GetSyncService(0)->SetSyncFeatureRequested();
  GetSyncService(0)->GetUserSettings()->SetInitialSyncFeatureSetupComplete(
      kSetSourceFromTest);
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion());
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureEnabled());

  // Make sure the password is now in the profile store, but *not* in the
  // account store anymore.
  password_manager::PasswordStoreInterface* profile_store =
      passwords_helper::GetProfilePasswordStoreInterface(0);
  EXPECT_EQ(passwords_helper::GetAllLogins(profile_store).size(), 1u);
  EXPECT_EQ(passwords_helper::GetAllLogins(account_store).size(), 0u);

  // Clear the primary account to put Sync into transport mode again.
  // Note: Clearing the primary account without also signing out isn't exposed
  // to the user, so this shouldn't happen. Still best to cover it here.
  signin::RevokeSyncConsent(
      IdentityManagerFactory::GetForProfile(GetProfile(0)));
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());

  // The account-storage opt-in is still present, so PASSWORDS should become
  // active.
  PasswordSyncActiveChecker(GetSyncService(0)).Wait();

  // Now the password should be in both stores: The profile store does *not* get
  // cleared when Sync gets disabled.
  EXPECT_EQ(passwords_helper::GetAllLogins(profile_store).size(), 1u);
  EXPECT_EQ(passwords_helper::GetAllLogins(account_store).size(), 1u);
}

// Regression test for crbug.com/1076378.
// TODO(b/327118794): Delete this test once implicit signin no longer exists.
IN_PROC_BROWSER_TEST_F(SingleClientPasswordsWithAccountStorageSyncTest,
                       EnablesPasswordSyncOnOptingInToSync) {
  AddTestPasswordToFakeServer();

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Setup Sync for an unconsented account (i.e. in transport mode).
  AccountInfo account_info =
      secondary_account_helper::ImplicitSignInUnconsentedAccount(
          GetProfile(0), &test_url_loader_factory_, "user@email.com");
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());

  // If signin is implicit, the user is not opted in to the account-scoped
  // password storage, so the passwords data type should *not* be active.
  ASSERT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PASSWORDS));

  // Turn on Sync-the-feature.
  secondary_account_helper::GrantSyncConsent(GetProfile(0), "user@email.com");
  GetSyncService(0)->SetSyncFeatureRequested();
  GetSyncService(0)->GetUserSettings()->SetInitialSyncFeatureSetupComplete(
      kSetSourceFromTest);
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion());
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureEnabled());

  // Now password sync should be active.
  EXPECT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PASSWORDS));
}

class SingleClientPasswordsWithAccountStorageExplicitSigninSyncTest
    : public SingleClientPasswordsWithAccountStorageSyncTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      switches::kExplicitBrowserSigninUIOnDesktop};
};

// In pending state, account storage is deleted and re-downloaded on reauth.
IN_PROC_BROWSER_TEST_F(
    SingleClientPasswordsWithAccountStorageExplicitSigninSyncTest,
    PendingState) {
  AddTestPasswordToFakeServer();

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Setup Sync in transport mode.
  secondary_account_helper::SignInUnconsentedAccount(
      GetProfile(0), &test_url_loader_factory_, "user@email.com");
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());

  // User is opted in to account-scoped password storage by default, wait for it
  // to become active.
  PasswordSyncActiveChecker(GetSyncService(0)).Wait();

  // Make sure the password showed up in the account store.
  password_manager::PasswordStoreInterface* account_store =
      passwords_helper::GetAccountPasswordStoreInterface(0);
  ASSERT_EQ(passwords_helper::GetAllLogins(account_store).size(), 1u);

  // Go to error state, sync stops.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(GetProfile(0));
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager,
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  ASSERT_TRUE(
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  PasswordSyncInactiveChecker(GetSyncService(0)).Wait();

  // Make sure the password is gone from the store.
  ASSERT_EQ(passwords_helper::GetAllLogins(account_store).size(), 0u);

  // Fix the authentication error, sync is available again.
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager,
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
      GoogleServiceAuthError::AuthErrorNone());
  PasswordSyncActiveChecker(GetSyncService(0)).Wait();

  // Make sure the password is back.
  ASSERT_EQ(passwords_helper::GetAllLogins(account_store).size(), 1u);
}

IN_PROC_BROWSER_TEST_F(
    SingleClientPasswordsWithAccountStorageExplicitSigninSyncTest,
    SyncPaused) {
  // Setup Sync with 2 passwords.
  ASSERT_TRUE(SetupClients());
  PasswordForm form0 = CreateTestPasswordForm(0);
  PasswordForm form1 = CreateTestPasswordForm(1);
  GetProfilePasswordStoreInterface(0)->AddLogin(form0);
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(ServerCountMatchStatusChecker(syncer::PASSWORDS, 1).Wait());
  std::vector<sync_pb::SyncEntity> server_passwords =
      GetFakeServer()->GetSyncEntitiesByDataType(syncer::PASSWORDS);
  ASSERT_EQ(1ul, server_passwords.size());
  sync_pb::SyncEntity entity0 = server_passwords[0];
  GetProfilePasswordStoreInterface(0)->AddLogin(form1);
  ASSERT_TRUE(ServerCountMatchStatusChecker(syncer::PASSWORDS, 2).Wait());
  server_passwords =
      GetFakeServer()->GetSyncEntitiesByDataType(syncer::PASSWORDS);
  ASSERT_EQ(2ul, server_passwords.size());
  ASSERT_TRUE(CommittedAllNudgedChangesChecker(GetSyncService(0)).Wait());

  // Go to sync paused.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(GetProfile(0));
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager,
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSync),
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  ASSERT_TRUE(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));
  PasswordSyncInactiveChecker(GetSyncService(0)).Wait();

  // Delete `form0` on the server.
  GetFakeServer()->InjectEntity(
      syncer::PersistentTombstoneEntity::CreateFromEntity(entity0));

  // Update `form1` locally.
  form1.password_value = u"updated_password";
  form1.date_created = base::Time::Now();
  GetProfilePasswordStoreInterface(0)->UpdateLogin(form1);

  // The passwords are still existing locally.
  PasswordFormsChecker(0, {form0, form1}).Wait();

  // Fix the authentication error, sync is available again.
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager,
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSync),
      GoogleServiceAuthError::AuthErrorNone());
  PasswordSyncActiveChecker(GetSyncService(0)).Wait();

  // `form0` has been deleted locally, only `form1` remains.
  PasswordFormsChecker(0, {form1}).Wait();

  // `form1` was updated on the server.
  EXPECT_TRUE(ServerPasswordsEqualityChecker(
                  {form1},
                  base::Base64Encode(GetFakeServer()->GetKeystoreKeys().back()),
                  syncer::KeyDerivationParams::CreateForPbkdf2())
                  .Wait());
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

IN_PROC_BROWSER_TEST_F(SingleClientPasswordsSyncTest,
                       PreservesUnsupportedFieldsDataOnCommits) {
  // Create an unsupported field with an unused tag.
  const std::string kUnsupportedField =
      CreateSerializedProtoField(/*field_number=*/999999, "unknown_field");

  // Create a password on the server with an unsupported field.
  sync_pb::PasswordSpecificsData password_data;
  password_data.set_origin("http://fake-site.com/");
  password_data.set_signon_realm("http://fake-site.com/");
  password_data.set_username_value("username");
  password_data.set_password_value("password");
  *password_data.mutable_unknown_fields() = kUnsupportedField;
  passwords_helper::InjectKeystoreEncryptedServerPassword(password_data,
                                                          GetFakeServer());

  // Sign in and enable Sync.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PASSWORDS));

  // Make a local update to the password.
  PasswordForm form;
  form.signon_realm = "http://fake-site.com/";
  form.url = GURL("http://fake-site.com/");
  form.username_value = u"username";
  form.password_value = u"new_password";
  form.date_created = base::Time::Now();
  GetProfilePasswordStoreInterface(0)->UpdateLogin(form);

  // Add an obsolete password to make sure that the server has received the
  // update. Otherwise, calling count match could finish before the local update
  // actually goes through (as there is already 1 password entity on the
  // server).
  GetProfilePasswordStoreInterface(0)->AddLogin(CreateTestPasswordForm(2));
  ASSERT_TRUE(ServerCountMatchStatusChecker(syncer::PASSWORDS, 2).Wait());

  // Check that the password was updated and the commit preserved the data for
  // an unsupported field.
  std::unique_ptr<syncer::CryptographerImpl> cryptographer =
      syncer::CryptographerImpl::FromSingleKeyForTesting(
          base::Base64Encode(fake_server_->GetKeystoreKeys().back()),
          syncer::KeyDerivationParams::CreateForPbkdf2());

  const std::vector<sync_pb::SyncEntity> entities =
      fake_server_->GetSyncEntitiesByDataType(syncer::PASSWORDS);
  EXPECT_THAT(entities,
              Contains(HasPasswordValueAndUnsupportedFields(
                  cryptographer.get(), "new_password", kUnsupportedField)));
}

IN_PROC_BROWSER_TEST_F(SingleClientPasswordsSyncTest,
                       PreservesUnsupportedNotesFieldsDataOnCommits) {
  // Create an unsupported field in the PasswordSpecificsData_Notes with an
  // unused tag.
  const std::string kUnsupportedNotesField =
      CreateSerializedProtoField(/*field_number=*/999999, "unknown_field 1");
  // Create an unsupported field in the PasswordSpecificsData_Notes_Note with an
  // unused tag. Since they are different protos, they can use the same
  // field_number.
  const std::string kUnsupportedNoteField =
      CreateSerializedProtoField(/*field_number=*/999999, "unknown_field 2");

  // Create a password on the server with an unsupported field in the notes
  // proto as well as the individual notes.
  sync_pb::PasswordSpecificsData password_data;
  password_data.set_origin("http://fake-site.com/");
  password_data.set_signon_realm("http://fake-site.com/");
  password_data.set_username_value("username-with-note");
  password_data.set_password_value("password");

  *password_data.mutable_notes()->mutable_unknown_fields() =
      kUnsupportedNotesField;

  sync_pb::PasswordSpecificsData_Notes_Note* note =
      password_data.mutable_notes()->add_note();
  note->set_value("note value");
  *note->mutable_unknown_fields() = kUnsupportedNoteField;

  passwords_helper::InjectKeystoreEncryptedServerPassword(password_data,
                                                          GetFakeServer());

  // Sign in and enable Sync.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PASSWORDS));

  // Make a local update to the password note.
  PasswordForm form;
  form.signon_realm = "http://fake-site.com/";
  form.url = GURL("http://fake-site.com/");
  form.username_value = u"username-with-note";
  form.password_value = u"password";
  form.notes.emplace_back(u"new note value",
                          /*date_created=*/base::Time::Now());
  GetProfilePasswordStoreInterface(0)->UpdateLogin(form);

  // Add an obsolete password to make sure that the server has received the
  // update. Otherwise, calling count match could finish before the local update
  // actually goes through (as there is already 1 password entity on the
  // server).
  GetProfilePasswordStoreInterface(0)->AddLogin(CreateTestPasswordForm(2));
  ASSERT_TRUE(ServerCountMatchStatusChecker(syncer::PASSWORDS, 2).Wait());

  // Check that the password note was updated and the commit preserved the data
  // for an unsupported field.
  std::unique_ptr<syncer::CryptographerImpl> cryptographer =
      syncer::CryptographerImpl::FromSingleKeyForTesting(
          base::Base64Encode(fake_server_->GetKeystoreKeys().back()),
          syncer::KeyDerivationParams::CreateForPbkdf2());

  const std::vector<sync_pb::SyncEntity> entities =
      fake_server_->GetSyncEntitiesByDataType(syncer::PASSWORDS);
  for (const sync_pb::SyncEntity& entity : entities) {
    // Find the password with the notes.
    sync_pb::PasswordSpecificsData decrypted;
    cryptographer->Decrypt(entity.specifics().password().encrypted(),
                           &decrypted);
    if (decrypted.username_value() != "username-with-note") {
      continue;
    }
    EXPECT_EQ(kUnsupportedNotesField, decrypted.notes().unknown_fields());
    ASSERT_EQ(1, decrypted.notes().note_size());
    sync_pb::PasswordSpecificsData_Notes_Note decrypted_note =
        decrypted.notes().note(0);
    EXPECT_EQ("new note value", decrypted_note.value());
    EXPECT_EQ(kUnsupportedNoteField, decrypted_note.unknown_fields());
  }
}

IN_PROC_BROWSER_TEST_F(SingleClientPasswordsSyncTest,
                       ClientReadsNotesFromTheBackup) {
  base::HistogramTester histogram_tester;

  const std::string& kEncryptionPassphrase =
      base::Base64Encode(GetFakeServer()->GetKeystoreKeys().back());

  // Add an entity on the server that does *not* have the notes field set.
  sync_pb::PasswordSpecificsData password_data;
  password_data.set_origin("http://fake-site.com/");
  password_data.set_signon_realm("http://fake-site.com/");
  password_data.set_username_value("username");
  password_data.set_password_value("password");
  passwords_helper::InjectKeystoreEncryptedServerPassword(password_data,
                                                          GetFakeServer());

  // Set the notes backup field to simulate a notes backup preserved by the
  // server upon a commit from a legacy client that didn't set the notes field
  // in the password specifics data.
  std::vector<sync_pb::SyncEntity> server_passwords =
      GetFakeServer()->GetSyncEntitiesByDataType(syncer::PASSWORDS);
  ASSERT_EQ(1ul, server_passwords.size());
  std::string entity_id = server_passwords[0].id_string();
  sync_pb::EntitySpecifics specifics = server_passwords[0].specifics();
  sync_pb::PasswordSpecifics* password_specifics = specifics.mutable_password();
  std::unique_ptr<syncer::CryptographerImpl> cryptographer =
      syncer::CryptographerImpl::FromSingleKeyForTesting(
          kEncryptionPassphrase,
          syncer::KeyDerivationParams::CreateForPbkdf2());
  sync_pb::PasswordSpecificsData_Notes notes;
  sync_pb::PasswordSpecificsData_Notes_Note* note = notes.add_note();
  note->set_value("some important note");
  cryptographer->Encrypt(notes,
                         password_specifics->mutable_encrypted_notes_backup());
  GetFakeServer()->ModifyEntitySpecifics(entity_id, specifics);

  // The server now should have one password entity.
  ASSERT_THAT(fake_server_->GetSyncEntitiesByDataType(syncer::PASSWORDS),
              testing::SizeIs(1));

  // Enable sync to download the passwords on the server.
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  PasswordSyncActiveChecker(GetSyncService(0)).Wait();

  // The local store should contain the note since the client should read the
  // backup when the note in the specifics data isn't set.
  EXPECT_THAT(
      passwords_helper::GetAllLogins(GetProfilePasswordStoreInterface(0)),
      Contains(Pointee(
          AllOf(Field(&PasswordForm::signon_realm, "http://fake-site.com/"),
                Field(&PasswordForm::username_value, u"username"),
                Field(&PasswordForm::password_value, u"password"),
                Field(&PasswordForm::notes,
                      Contains(Field(&password_manager::PasswordNote::value,
                                     u"some important note")))))));
  histogram_tester.ExpectUniqueSample("Sync.PasswordNotesStateInUpdate",
                                      /*kSetOnlyInBackup*/ 2, 1);
}

IN_PROC_BROWSER_TEST_F(SingleClientPasswordsSyncTest, Delete) {
  ASSERT_TRUE(SetupClients());

  const PasswordForm form0 = CreateTestPasswordForm(0);
  GetProfilePasswordStoreInterface(0)->AddLogin(form0);

  ASSERT_TRUE(SetupSync());
  ASSERT_EQ(
      1ul,
      GetFakeServer()->GetSyncEntitiesByDataType(syncer::PASSWORDS).size());

  const base::Location kDeletionLocation = FROM_HERE;
  GetProfilePasswordStoreInterface(0)->RemoveLogin(kDeletionLocation, form0);

  // Wait until there are no passwords in the FakeServer.
  EXPECT_TRUE(ServerPasswordsEqualityChecker(
                  {}, "", syncer::KeyDerivationParams::CreateForPbkdf2())
                  .Wait());

  EXPECT_THAT(
      GetFakeServer()->GetCommittedDeletionOrigins(syncer::DataType::PASSWORDS),
      ElementsAre(syncer::MatchesDeletionOrigin(
          version_info::GetVersionNumber(), kDeletionLocation)));
}

}  // namespace
