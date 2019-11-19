// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/macros.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/test/integration/encryption_helper.h"
#include "chrome/browser/sync/test/integration/feature_toggler.h"
#include "chrome/browser/sync/test/integration/passwords_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/secondary_account_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/sync/password_sync_bridge.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/time.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/syncable/directory_cryptographer.h"
#include "content/public/test/test_launcher.h"

namespace {

using passwords_helper::AddLogin;
using passwords_helper::CreateTestPasswordForm;
using passwords_helper::GetPasswordCount;
using passwords_helper::GetPasswordStore;
using passwords_helper::GetVerifierPasswordCount;
using passwords_helper::GetVerifierPasswordStore;
using passwords_helper::ProfileContainsSamePasswordFormsAsVerifier;

using autofill::PasswordForm;

using testing::ElementsAre;
using testing::IsEmpty;

const syncer::SyncFirstSetupCompleteSource kSetSourceFromTest =
    syncer::SyncFirstSetupCompleteSource::BASIC_FLOW;

syncer::KeyParams KeystoreKeyParams(const std::string& key) {
  // Due to mis-encode of keystore keys to base64 we have to always encode such
  // keys to provide backward compatibility.
  std::string encoded_key;
  base::Base64Encode(key, &encoded_key);
  return {syncer::KeyDerivationParams::CreateForPbkdf2(),
          std::move(encoded_key)};
}

sync_pb::PasswordSpecifics EncryptPasswordSpecifics(
    const syncer::KeyParams& key_params,
    const sync_pb::PasswordSpecificsData& password_data) {
  syncer::DirectoryCryptographer cryptographer;
  cryptographer.AddKey(key_params);

  sync_pb::PasswordSpecifics encrypted_specifics;
  encrypted_specifics.mutable_unencrypted_metadata()->set_url(
      password_data.signon_realm());
  bool result = cryptographer.Encrypt(password_data,
                                      encrypted_specifics.mutable_encrypted());
  DCHECK(result);
  return encrypted_specifics;
}

class SingleClientPasswordsSyncTest : public FeatureToggler, public SyncTest {
 public:
  SingleClientPasswordsSyncTest()
      : FeatureToggler(switches::kSyncUSSPasswords), SyncTest(SINGLE_CLIENT) {}
  ~SingleClientPasswordsSyncTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientPasswordsSyncTest);
};

IN_PROC_BROWSER_TEST_P(SingleClientPasswordsSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  PasswordForm form = CreateTestPasswordForm(0);
  AddLogin(GetVerifierPasswordStore(), form);
  ASSERT_EQ(1, GetVerifierPasswordCount());
  AddLogin(GetPasswordStore(0), form);
  ASSERT_EQ(1, GetPasswordCount(0));

  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(ProfileContainsSamePasswordFormsAsVerifier(0));
  ASSERT_EQ(1, GetPasswordCount(0));
}

// Verifies that committed passwords contain the appropriate proto fields, and
// in particular lack some others that could potentially contain unencrypted
// data. In this test, custom passphrase is NOT set.
IN_PROC_BROWSER_TEST_P(SingleClientPasswordsSyncTest,
                       CommitWithoutCustomPassphrase) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  PasswordForm form = CreateTestPasswordForm(0);
  AddLogin(GetVerifierPasswordStore(), form);
  ASSERT_EQ(1, GetVerifierPasswordCount());
  AddLogin(GetPasswordStore(0), form);
  ASSERT_EQ(1, GetPasswordCount(0));
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  const std::vector<sync_pb::SyncEntity> entities =
      fake_server_->GetSyncEntitiesByModelType(syncer::PASSWORDS);
  ASSERT_EQ(1U, entities.size());
  EXPECT_EQ("", entities[0].non_unique_name());
  EXPECT_TRUE(entities[0].specifics().password().has_encrypted());
  EXPECT_FALSE(
      entities[0].specifics().password().has_client_only_encrypted_data());
  EXPECT_TRUE(entities[0].specifics().password().has_unencrypted_metadata());
  EXPECT_TRUE(
      entities[0].specifics().password().unencrypted_metadata().has_url());
}

// Same as above but with custom passphrase set, which requires to prune commit
// data even further.
IN_PROC_BROWSER_TEST_P(SingleClientPasswordsSyncTest,
                       CommitWithCustomPassphrase) {
  SetEncryptionPassphraseForClient(/*index=*/0, "hunter2");
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  PasswordForm form = CreateTestPasswordForm(0);
  AddLogin(GetVerifierPasswordStore(), form);
  ASSERT_EQ(1, GetVerifierPasswordCount());
  AddLogin(GetPasswordStore(0), form);
  ASSERT_EQ(1, GetPasswordCount(0));
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  const std::vector<sync_pb::SyncEntity> entities =
      fake_server_->GetSyncEntitiesByModelType(syncer::PASSWORDS);
  ASSERT_EQ(1U, entities.size());
  EXPECT_EQ("", entities[0].non_unique_name());
  EXPECT_TRUE(entities[0].specifics().password().has_encrypted());
  EXPECT_FALSE(
      entities[0].specifics().password().has_client_only_encrypted_data());
  EXPECT_FALSE(entities[0].specifics().password().has_unencrypted_metadata());
}

// Tests the scenario when a syncing user enables a custom passphrase. PASSWORDS
// should be recommitted with the new encryption key.
IN_PROC_BROWSER_TEST_P(SingleClientPasswordsSyncTest,
                       ReencryptsDataWhenPassphraseIsSet) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(ServerNigoriChecker(GetSyncService(0), fake_server_.get(),
                                  syncer::PassphraseType::kKeystorePassphrase)
                  .Wait());

  PasswordForm form = CreateTestPasswordForm(0);
  AddLogin(GetVerifierPasswordStore(), form);
  ASSERT_EQ(1, GetVerifierPasswordCount());
  AddLogin(GetPasswordStore(0), form);
  ASSERT_EQ(1, GetPasswordCount(0));
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  std::string prior_encryption_key_name;
  {
    const std::vector<sync_pb::SyncEntity> entities =
        fake_server_->GetSyncEntitiesByModelType(syncer::PASSWORDS);
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
  ASSERT_TRUE(ServerNigoriChecker(GetSyncService(0), fake_server_.get(),
                                  syncer::PassphraseType::kCustomPassphrase)
                  .Wait());
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  const std::vector<sync_pb::SyncEntity> entities =
      fake_server_->GetSyncEntitiesByModelType(syncer::PASSWORDS);
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

IN_PROC_BROWSER_TEST_P(SingleClientPasswordsSyncTest,
                       PRE_PersistProgressMarkerOnRestart) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  PasswordForm form = CreateTestPasswordForm(0);
  AddLogin(GetPasswordStore(0), form);
  ASSERT_EQ(1, GetPasswordCount(0));
  // Setup sync, wait for its completion, and make sure changes were synced.
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  // Upon a local creation, the received update will be seen as reflection and
  // get counted as incremental update.
  EXPECT_EQ(
      1, histogram_tester.GetBucketCount("Sync.ModelTypeEntityChange3.PASSWORD",
                                         /*REMOTE_NON_INITIAL_UPDATE=*/4));
}

IN_PROC_BROWSER_TEST_P(SingleClientPasswordsSyncTest,
                       PersistProgressMarkerOnRestart) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  ASSERT_EQ(1, GetPasswordCount(0));
#if defined(CHROMEOS)
  // signin::SetRefreshTokenForPrimaryAccount() is needed on ChromeOS in order
  // to get a non-empty refresh token on startup.
  GetClient(0)->SignInPrimaryAccount();
#endif  // defined(CHROMEOS)
  ASSERT_TRUE(GetClient(0)->AwaitEngineInitialization());

  // After restart, the last sync cycle snapshot should be empty. Once a sync
  // request happened (e.g. by a poll), that snapshot is populated. We use the
  // following checker to simply wait for an non-empty snapshot.
  EXPECT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  // If that metadata hasn't been properly persisted, the password stored on the
  // server will be received at the client as an initial update or an
  // incremental once.
  EXPECT_EQ(
      0, histogram_tester.GetBucketCount("Sync.ModelTypeEntityChange3.PASSWORD",
                                         /*REMOTE_INITIAL_UPDATE=*/5));
  EXPECT_EQ(
      0, histogram_tester.GetBucketCount("Sync.ModelTypeEntityChange3.PASSWORD",
                                         /*REMOTE_NON_INITIAL_UPDATE=*/4));
}

INSTANTIATE_TEST_SUITE_P(USS,
                         SingleClientPasswordsSyncTest,
                         ::testing::Values(false, true));

class SingleClientPasswordsSyncUssMigratorTest : public SyncTest {
 public:
  SingleClientPasswordsSyncUssMigratorTest() : SyncTest(SINGLE_CLIENT) {}
  ~SingleClientPasswordsSyncUssMigratorTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientPasswordsSyncUssMigratorTest);
};

class SingleClientPasswordsSyncUssMigratorTestWithUssTransition
    : public SingleClientPasswordsSyncUssMigratorTest {
 public:
  SingleClientPasswordsSyncUssMigratorTestWithUssTransition() {
    if (content::IsPreTest())
      feature_list_.InitAndDisableFeature(switches::kSyncUSSPasswords);
    else
      feature_list_.InitAndEnableFeature(switches::kSyncUSSPasswords);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Creates and syncs two passwords before USS being enabled.
IN_PROC_BROWSER_TEST_F(
    SingleClientPasswordsSyncUssMigratorTestWithUssTransition,
    PRE_ExerciseUssMigrator) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  AddLogin(GetPasswordStore(0), CreateTestPasswordForm(0));
  AddLogin(GetPasswordStore(0), CreateTestPasswordForm(1));
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_EQ(2, GetPasswordCount(0));
}

// Now that local passwords, the local sync directory and the sever are
// populated with two passwords, USS is enabled for passwords.
IN_PROC_BROWSER_TEST_F(
    SingleClientPasswordsSyncUssMigratorTestWithUssTransition,
    ExerciseUssMigrator) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  ASSERT_EQ(2, GetPasswordCount(0));
#if defined(CHROMEOS)
  // signin::SetRefreshTokenForPrimaryAccount() is needed on ChromeOS in order
  // to get a non-empty refresh token on startup.
  GetClient(0)->SignInPrimaryAccount();
#endif  // defined(CHROMEOS)
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion());
  ASSERT_EQ(2, GetPasswordCount(0));

  EXPECT_EQ(1, histogram_tester.GetBucketCount(
                   "Sync.USSMigrationSuccess",
                   static_cast<int>(
                       syncer::ModelTypeHistogramValue(syncer::PASSWORDS))));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Sync.USSMigrationEntityCount.PASSWORD"),
      ElementsAre(base::Bucket(/*min=*/2, /*count=*/1)));
  EXPECT_THAT(histogram_tester.GetAllSamples("Sync.DataTypeStartFailures2"),
              IsEmpty());
  EXPECT_EQ(
      0, histogram_tester.GetBucketCount("Sync.ModelTypeEntityChange3.PASSWORD",
                                         /*REMOTE_INITIAL_UPDATE=*/5));
}

class SingleClientPasswordsWithAccountStorageSyncTest : public SyncTest {
 public:
  SingleClientPasswordsWithAccountStorageSyncTest() : SyncTest(SINGLE_CLIENT) {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{switches::kSyncUSSPasswords,
                              password_manager::features::
                                  kEnablePasswordsAccountStorage},
        /*disabled_features=*/{});
  }
  ~SingleClientPasswordsWithAccountStorageSyncTest() override {}

  void SetUpInProcessBrowserTestFixture() override {
    test_signin_client_factory_ =
        secondary_account_helper::SetUpSigninClient(&test_url_loader_factory_);
  }

  void SetUpOnMainThread() override {
#if defined(OS_CHROMEOS)
    secondary_account_helper::InitNetwork();
#endif  // defined(OS_CHROMEOS)
    SyncTest::SetUpOnMainThread();

    AddKeystoreNigoriToFakeServer();
  }

  void AddKeystoreNigoriToFakeServer() {
    const std::vector<std::string>& keystore_keys =
        GetFakeServer()->GetKeystoreKeys();
    ASSERT_TRUE(keystore_keys.size() == 1);
    const syncer::KeyParams key_params =
        KeystoreKeyParams(keystore_keys.back());

    sync_pb::NigoriSpecifics nigori;
    nigori.set_keybag_is_frozen(true);
    nigori.set_keystore_migration_time(1U);
    nigori.set_encrypt_everything(false);
    nigori.set_passphrase_type(sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE);

    syncer::DirectoryCryptographer cryptographer;
    ASSERT_TRUE(cryptographer.AddKey(key_params));
    ASSERT_TRUE(cryptographer.AddNonDefaultKey(key_params));
    ASSERT_TRUE(cryptographer.GetKeys(nigori.mutable_encryption_keybag()));

    syncer::DirectoryCryptographer keystore_cryptographer;
    ASSERT_TRUE(keystore_cryptographer.AddKey(key_params));
    ASSERT_TRUE(keystore_cryptographer.EncryptString(
        cryptographer.GetDefaultNigoriKeyData(),
        nigori.mutable_keystore_decryptor_token()));

    encryption_helper::SetNigoriInFakeServer(GetFakeServer(), nigori);
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

    fake_server_->InjectEntity(
        syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
            /*non_unique_name=*/"encrypted", /*client_tag=*/
            password_manager::PasswordSyncBridge::ComputeClientTagForTesting(
                password_data),
            CreateSpecifics(password_data),
            /*creation_time=*/syncer::TimeToProtoTime(base::Time::Now()),
            /*last_modified_time=*/syncer::TimeToProtoTime(base::Time::Now())));
  }

 private:
  sync_pb::EntitySpecifics CreateSpecifics(
      const sync_pb::PasswordSpecificsData& password_data) const {
    const syncer::KeyParams key_params =
        KeystoreKeyParams(GetFakeServer()->GetKeystoreKeys().back());

    sync_pb::EntitySpecifics specifics;
    *specifics.mutable_password() =
        EncryptPasswordSpecifics(key_params, password_data);

    return specifics;
  }

  base::test::ScopedFeatureList feature_list_;

  secondary_account_helper::ScopedSigninClientFactory
      test_signin_client_factory_;

  DISALLOW_COPY_AND_ASSIGN(SingleClientPasswordsWithAccountStorageSyncTest);
};

// Sanity check: For Sync-the-feature, password data still ends up in the
// profile database.
IN_PROC_BROWSER_TEST_F(SingleClientPasswordsWithAccountStorageSyncTest,
                       StoresDataForSyncingPrimaryAccountInProfileDB) {
  AddTestPasswordToFakeServer();

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Sign in and enable Sync.
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  GetSyncService(0)->GetUserSettings()->SetSyncRequested(true);
  GetSyncService(0)->GetUserSettings()->SetFirstSetupComplete(
      kSetSourceFromTest);
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());

  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PASSWORDS));

  // Make sure the password showed up in the account store and not in the
  // profile store.
  password_manager::PasswordStore* profile_store =
      passwords_helper::GetPasswordStore(0);
  EXPECT_EQ(passwords_helper::GetAllLogins(profile_store).size(), 1u);

  password_manager::PasswordStore* account_store =
      passwords_helper::GetAccountPasswordStore(0);
  EXPECT_EQ(passwords_helper::GetAllLogins(account_store).size(), 0u);
}

IN_PROC_BROWSER_TEST_F(SingleClientPasswordsWithAccountStorageSyncTest,
                       StoresDataForNonSyncingPrimaryAccountInAccountDB) {
  AddTestPasswordToFakeServer();

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

#if defined(OS_CHROMEOS)
  // On ChromeOS, Sync-the-feature gets started automatically once a primary
  // account is signed in. To prevent that, explicitly set SyncRequested to
  // false.
  GetSyncService(0)->GetUserSettings()->SetSyncRequested(false);
#endif  // defined(OS_CHROMEOS)

  // Setup a primary account, but don't actually enable Sync-the-feature (so
  // that Sync will start in transport mode).
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());

  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PASSWORDS));

  // Make sure the password showed up in the account store and not in the
  // profile store.
  password_manager::PasswordStore* profile_store =
      passwords_helper::GetPasswordStore(0);
  EXPECT_EQ(passwords_helper::GetAllLogins(profile_store).size(), 0u);

  password_manager::PasswordStore* account_store =
      passwords_helper::GetAccountPasswordStore(0);
  EXPECT_EQ(passwords_helper::GetAllLogins(account_store).size(), 1u);
}

// The unconsented primary account isn't supported on ChromeOS (see
// IdentityManager::ComputeUnconsentedPrimaryAccountInfo()) so Sync won't start
// up for a secondary account.
#if !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SingleClientPasswordsWithAccountStorageSyncTest,
                       StoresDataForSecondaryAccountInAccountDB) {
  AddTestPasswordToFakeServer();

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Setup Sync for a secondary account (i.e. in transport mode).
  secondary_account_helper::SignInSecondaryAccount(
      GetProfile(0), &test_url_loader_factory_, "user@email.com");
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());

  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PASSWORDS));

  // Make sure the password showed up in the account store and not in the
  // profile store.
  password_manager::PasswordStore* profile_store =
      passwords_helper::GetPasswordStore(0);
  EXPECT_EQ(passwords_helper::GetAllLogins(profile_store).size(), 0u);

  password_manager::PasswordStore* account_store =
      passwords_helper::GetAccountPasswordStore(0);
  EXPECT_EQ(passwords_helper::GetAllLogins(account_store).size(), 1u);
}
#endif  // !defined(OS_CHROMEOS)

// ChromeOS does not support signing out of a primary account.
#if !defined(OS_CHROMEOS)
// Sanity check: The profile database should *not* get cleared on signout.
IN_PROC_BROWSER_TEST_F(SingleClientPasswordsWithAccountStorageSyncTest,
                       DoesNotClearProfileDBOnSignout) {
  AddTestPasswordToFakeServer();

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Sign in and enable Sync.
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  GetSyncService(0)->GetUserSettings()->SetSyncRequested(true);
  GetSyncService(0)->GetUserSettings()->SetFirstSetupComplete(
      kSetSourceFromTest);
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion());

  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureEnabled());

  // Make sure the password showed up in the profile store.
  password_manager::PasswordStore* profile_store =
      passwords_helper::GetPasswordStore(0);
  ASSERT_EQ(passwords_helper::GetAllLogins(profile_store).size(), 1u);

  // Sign out again.
  GetClient(0)->SignOutPrimaryAccount();
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());

  // Make sure the password is still in the store.
  ASSERT_EQ(passwords_helper::GetAllLogins(profile_store).size(), 1u);
}
#endif  // !defined(OS_CHROMEOS)

// The unconsented primary account isn't supported on ChromeOS (see
// IdentityManager::ComputeUnconsentedPrimaryAccountInfo()) so Sync won't start
// up for a secondary account.
#if !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SingleClientPasswordsWithAccountStorageSyncTest,
                       ClearsAccountDBOnSignout) {
  AddTestPasswordToFakeServer();

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Setup Sync for a secondary account (i.e. in transport mode).
  AccountInfo account_info = secondary_account_helper::SignInSecondaryAccount(
      GetProfile(0), &test_url_loader_factory_, "user@email.com");
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());

  // Make sure the password showed up in the account store.
  password_manager::PasswordStore* account_store =
      passwords_helper::GetAccountPasswordStore(0);
  ASSERT_EQ(passwords_helper::GetAllLogins(account_store).size(), 1u);

  // Sign out again.
  secondary_account_helper::SignOutSecondaryAccount(
      GetProfile(0), &test_url_loader_factory_, account_info.account_id);

  // Make sure the password is gone from the store.
  ASSERT_EQ(passwords_helper::GetAllLogins(account_store).size(), 0u);
}
#endif  // !defined(OS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(SingleClientPasswordsWithAccountStorageSyncTest,
                       SwitchesStoresOnEnablingSync) {
  AddTestPasswordToFakeServer();

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // On ChromeOS, Sync-the-feature starts automatically as soon as a primary
  // account is signed in. To prevent that, explicitly set SyncRequested to
  // false on ChromeOS.
#if defined(OS_CHROMEOS)
  GetSyncService(0)->GetUserSettings()->SetSyncRequested(false);
#endif  // !defined(OS_CHROMEOS)

  // Sign in to a primary account, but don't enable Sync-the-feature.
  // Note: This state shouldn't actually be reachable (the flow for setting a
  // primary account also enables Sync), but still best to cover it here.
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());

  // Make sure the password showed up in the account store.
  password_manager::PasswordStore* account_store =
      passwords_helper::GetAccountPasswordStore(0);
  ASSERT_EQ(passwords_helper::GetAllLogins(account_store).size(), 1u);

  // Turn on Sync-the-feature.
  GetSyncService(0)->GetUserSettings()->SetSyncRequested(true);
  GetSyncService(0)->GetUserSettings()->SetFirstSetupComplete(
      kSetSourceFromTest);
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion());
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureEnabled());

  // Make sure the password is now in the profile store, but *not* in the
  // account store anymore.
  password_manager::PasswordStore* profile_store =
      passwords_helper::GetPasswordStore(0);
  EXPECT_EQ(passwords_helper::GetAllLogins(profile_store).size(), 1u);
  EXPECT_EQ(passwords_helper::GetAllLogins(account_store).size(), 0u);

  // Turn off Sync-the-feature again.
  // Note: Turning Sync off without signing out isn't actually exposed to the
  // user, so this generally shouldn't happen. Still best to cover it here.
  GetSyncService(0)->GetUserSettings()->SetSyncRequested(false);
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());

  // Now the password should be in both stores: The profile store does *not* get
  // cleared when Sync gets disabled.
  EXPECT_EQ(passwords_helper::GetAllLogins(profile_store).size(), 1u);
  EXPECT_EQ(passwords_helper::GetAllLogins(account_store).size(), 1u);
}

// The unconsented primary account isn't supported on ChromeOS (see
// IdentityManager::ComputeUnconsentedPrimaryAccountInfo()) so Sync won't start
// up for a secondary account.
#if !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SingleClientPasswordsWithAccountStorageSyncTest,
                       SwitchesStoresOnMakingAccountPrimary) {
  AddTestPasswordToFakeServer();

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Setup Sync for a secondary account (i.e. in transport mode).
  AccountInfo account_info = secondary_account_helper::SignInSecondaryAccount(
      GetProfile(0), &test_url_loader_factory_, "user@email.com");
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());

  // Make sure the password showed up in the account store.
  password_manager::PasswordStore* account_store =
      passwords_helper::GetAccountPasswordStore(0);
  ASSERT_EQ(passwords_helper::GetAllLogins(account_store).size(), 1u);

  // Make the account primary and turn on Sync-the-feature.
  secondary_account_helper::MakeAccountPrimary(GetProfile(0), "user@email.com");
  GetSyncService(0)->GetUserSettings()->SetSyncRequested(true);
  GetSyncService(0)->GetUserSettings()->SetFirstSetupComplete(
      kSetSourceFromTest);
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion());
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureEnabled());

  // Make sure the password is now in the profile store, but *not* in the
  // account store anymore.
  password_manager::PasswordStore* profile_store =
      passwords_helper::GetPasswordStore(0);
  EXPECT_EQ(passwords_helper::GetAllLogins(profile_store).size(), 1u);
  EXPECT_EQ(passwords_helper::GetAllLogins(account_store).size(), 0u);

  // Clear the primary account to put Sync into transport mode again.
  // Note: Clearing the primary account without also signing out isn't exposed
  // to the user, so this shouldn't happen. Still best to cover it here.
  signin::ClearPrimaryAccount(
      IdentityManagerFactory::GetForProfile(GetProfile(0)),
      signin::ClearPrimaryAccountPolicy::KEEP_ALL_ACCOUNTS);
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());

  // Now the password should be in both stores: The profile store does *not* get
  // cleared when Sync gets disabled.
  EXPECT_EQ(passwords_helper::GetAllLogins(profile_store).size(), 1u);
  EXPECT_EQ(passwords_helper::GetAllLogins(account_store).size(), 1u);
}
#endif  // !defined(OS_CHROMEOS)

}  // namespace
