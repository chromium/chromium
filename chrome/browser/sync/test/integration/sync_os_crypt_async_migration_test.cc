// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sync/test/integration/passwords_helper.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/sync/base/features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using passwords_helper::GetAccountPasswordStoreInterface;
using passwords_helper::GetPasswordCount;
using passwords_helper::GetProfilePasswordStoreInterface;

// This test simulates a migration scenario for the kSyncUseOsCryptAsync
// feature. It is structured as a PRE_PRE_ / PRE_ / regular test to check that
// passwords synced with different states of the feature flag can be correctly
// decrypted and read by the client. The sequence is:
// 1. PRE_PRE_Migrate: kSyncUseOsCryptAsync disabled. A password is added.
// 2. PRE_Migrate: kSyncUseOsCryptAsync enabled. Another password is added.
// 3. Migrate: kSyncUseOsCryptAsync disabled again. Verifies that both
//    passwords are present and readable.
// Note that OSCrypt itself is stateless, but this tests that the encryption
// keys are managed correctly and that data remains readable across migrations.
class SyncOSCryptAsyncMigrationTest
    : public SyncTest,
      public testing::WithParamInterface<SyncTest::SetupSyncMode> {
 public:
  SyncOSCryptAsyncMigrationTest() : SyncTest(SINGLE_CLIENT) {}
  ~SyncOSCryptAsyncMigrationTest() override = default;

  SyncTest::SetupSyncMode GetSetupSyncMode() const override {
    return GetParam();
  }

  void SetUpInProcessBrowserTestFixture() override {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    if (GetSetupSyncMode() == SetupSyncMode::kSyncTransportOnly) {
      enabled_features.push_back(syncer::kReplaceSyncPromosWithSignInPromos);
    }

    switch (GetTestPreCount()) {
      case 2:  // PRE_PRE_Migrate
        disabled_features.push_back(syncer::kSyncUseOsCryptAsync);
        break;
      case 1:  // PRE_Migrate
        enabled_features.push_back(syncer::kSyncUseOsCryptAsync);
        break;
      case 0:  // Migrate
        disabled_features.push_back(syncer::kSyncUseOsCryptAsync);
        break;
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
    SyncTest::SetUpInProcessBrowserTestFixture();
  }

  password_manager::PasswordForm::Store GetStoreType() const {
    switch (GetSetupSyncMode()) {
      case SetupSyncMode::kSyncTransportOnly:
        return password_manager::PasswordForm::Store::kAccountStore;
      case SetupSyncMode::kSyncTheFeature:
        return password_manager::PasswordForm::Store::kProfileStore;
    }
  }

  password_manager::PasswordForm CreateTestPasswordForm(int index) {
    return passwords_helper::CreateTestPasswordForm(index, GetStoreType());
  }

  password_manager::PasswordStoreInterface* GetPasswordStore() {
    return passwords_helper::GetPasswordStoreInterface(0, GetStoreType());
  }

  int GetPasswordStoreCount() { return GetPasswordCount(0, GetStoreType()); }

  std::vector<password_manager::PasswordForm> GetAllPasswords() {
    std::vector<std::unique_ptr<password_manager::PasswordForm>> logins =
        passwords_helper::GetLogins(GetPasswordStore());
    std::vector<password_manager::PasswordForm> passwords;
    for (const auto& login : logins) {
      passwords.push_back(*login);
    }
    return passwords;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(SyncOSCryptAsyncMigrationTest, PRE_PRE_Migrate) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_EQ(GetPasswordStoreCount(), 0);

  GetPasswordStore()->AddLogin(CreateTestPasswordForm(0));

  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
}

IN_PROC_BROWSER_TEST_P(SyncOSCryptAsyncMigrationTest, PRE_Migrate) {
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_EQ(GetPasswordStoreCount(), 1);

  GetPasswordStore()->AddLogin(CreateTestPasswordForm(1));

  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
}

IN_PROC_BROWSER_TEST_P(SyncOSCryptAsyncMigrationTest, Migrate) {
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  GetSyncService(0)->TriggerRefresh(
      syncer::SyncService::TriggerRefreshSource::kUnknown, {syncer::PASSWORDS});
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_EQ(GetPasswordStoreCount(), 2);

  std::vector<password_manager::PasswordForm> passwords = GetAllPasswords();
  ASSERT_EQ(passwords.size(), 2u);

  // Sort by username to have a deterministic order.
  std::sort(passwords.begin(), passwords.end(),
            [](const password_manager::PasswordForm& a,
               const password_manager::PasswordForm& b) {
              return a.username_value < b.username_value;
            });

  // If kSyncUseOsCryptAsync were to use an incorrect key, the decryption of the
  // second password (added while the feature was enabled) would fail, causing
  // its `password_value` to be empty and the expectation below to fail.
  EXPECT_EQ(passwords[0].username_value, u"username0");
  EXPECT_EQ(passwords[0].password_value, u"password0");
  EXPECT_EQ(passwords[1].username_value, u"username1");
  EXPECT_EQ(passwords[1].password_value, u"password1");
}

INSTANTIATE_TEST_SUITE_P(All,
                         SyncOSCryptAsyncMigrationTest,
                         GetSyncTestModes(),
                         testing::PrintToStringParamName());

}  // namespace
