// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "chrome/browser/sync/test/integration/encryption_helper.h"
#include "chrome/browser/sync/test/integration/feature_toggler.h"
#include "chrome/browser/sync/test/integration/passwords_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/sync/driver/sync_driver_switches.h"

using passwords_helper::AddLogin;
using passwords_helper::CreateTestPasswordForm;
using passwords_helper::GetPasswordCount;
using passwords_helper::GetPasswordStore;
using passwords_helper::GetVerifierPasswordCount;
using passwords_helper::GetVerifierPasswordStore;
using passwords_helper::ProfileContainsSamePasswordFormsAsVerifier;

using autofill::PasswordForm;

class SingleClientPasswordsSyncTest : public FeatureToggler, public SyncTest {
 public:
  SingleClientPasswordsSyncTest()
      : FeatureToggler(switches::kSyncPseudoUSSPasswords),
        SyncTest(SINGLE_CLIENT) {}
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
                                  syncer::PassphraseType::KEYSTORE_PASSPHRASE)
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

  GetSyncService(0)->SetEncryptionPassphrase("hunter2");
  ASSERT_TRUE(ServerNigoriChecker(GetSyncService(0), fake_server_.get(),
                                  syncer::PassphraseType::CUSTOM_PASSPHRASE)
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

INSTANTIATE_TEST_CASE_P(USS,
                        SingleClientPasswordsSyncTest,
                        ::testing::Values(false, true));
