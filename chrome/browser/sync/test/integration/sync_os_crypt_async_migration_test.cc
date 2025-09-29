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

using passwords_helper::CreateTestPasswordForm;
using passwords_helper::GetAllPasswordsForProfile;
using passwords_helper::GetPasswordCount;
using passwords_helper::GetProfilePasswordStoreInterface;

class SyncOSCryptAsyncMigrationTest : public SyncTest {
 public:
  SyncOSCryptAsyncMigrationTest() : SyncTest(SINGLE_CLIENT) {}
  ~SyncOSCryptAsyncMigrationTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    switch (GetTestPreCount()) {
      case 2:  // PRE_PRE_Migrate
        scoped_feature_list_.InitAndDisableFeature(
            syncer::kSyncUseOsCryptAsync);
        break;
      case 1:  // PRE_Migrate
        scoped_feature_list_.InitAndEnableFeature(syncer::kSyncUseOsCryptAsync);
        break;
      case 0:  // Migrate
        scoped_feature_list_.InitAndDisableFeature(
            syncer::kSyncUseOsCryptAsync);
        break;
    }
    SyncTest::SetUpInProcessBrowserTestFixture();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SyncOSCryptAsyncMigrationTest, PRE_PRE_Migrate) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_EQ(GetPasswordCount(0), 0);

  GetProfilePasswordStoreInterface(0)->AddLogin(CreateTestPasswordForm(0));

  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
}

IN_PROC_BROWSER_TEST_F(SyncOSCryptAsyncMigrationTest, PRE_Migrate) {
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion());
  ASSERT_EQ(GetPasswordCount(0), 1);

  GetProfilePasswordStoreInterface(0)->AddLogin(CreateTestPasswordForm(1));

  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
}

IN_PROC_BROWSER_TEST_F(SyncOSCryptAsyncMigrationTest, Migrate) {
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion());
  GetSyncService(0)->TriggerRefresh(
      syncer::SyncService::TriggerRefreshSource::kUnknown, {syncer::PASSWORDS});
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_EQ(GetPasswordCount(0), 2);

  std::vector<password_manager::PasswordForm> passwords =
      GetAllPasswordsForProfile(0);
  ASSERT_EQ(passwords.size(), 2u);

  // Sort by username to have a deterministic order.
  std::sort(passwords.begin(), passwords.end(),
            [](const password_manager::PasswordForm& a,
               const password_manager::PasswordForm& b) {
              return a.username_value < b.username_value;
            });

  EXPECT_EQ(passwords[0].username_value, u"username0");
  EXPECT_EQ(passwords[0].password_value, u"password0");
  EXPECT_EQ(passwords[1].username_value, u"username1");
  EXPECT_EQ(passwords[1].password_value, u"password1");
}

}  // namespace
