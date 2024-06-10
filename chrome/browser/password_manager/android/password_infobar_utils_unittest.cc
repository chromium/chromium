// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_infobar_utils.h"

#include "base/test/task_environment.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync/test/test_sync_user_settings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

const char kTestEmail[] = "foo@gmail.com";

class PasswordInfobarUtilsTest : public testing::Test {
 public:
  PasswordInfobarUtilsTest() {
    // `identity_test_environment_` starts signed-out by default, but
    // `sync_service_` starts signed-in, make them consistent.
    sync_service_.SetSignedOut();
  }

  void SignInWithoutSync() {
    AccountInfo account_info =
        identity_test_environment_.MakePrimaryAccountAvailable(
            kTestEmail, signin::ConsentLevel::kSignin);
    sync_service_.SetSignedInWithoutSyncFeature(account_info);
  }

  void SignInWithSync() {
    AccountInfo account_info =
        identity_test_environment_.MakePrimaryAccountAvailable(
            kTestEmail, signin::ConsentLevel::kSync);
    sync_service_.SetSignedInWithSyncFeatureOn(account_info);
  }

  syncer::SyncService* sync_service() { return &sync_service_; }

  signin::IdentityManager* identity_manager() {
    return identity_test_environment_.identity_manager();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  syncer::TestSyncService sync_service_;
  signin::IdentityTestEnvironment identity_test_environment_;
};

TEST_F(PasswordInfobarUtilsTest, SignedOut) {
  EXPECT_EQ(
      GetAccountInfoForPasswordMessages(sync_service(), identity_manager()),
      std::nullopt);
  EXPECT_EQ(GetDisplayableAccountName(sync_service(), identity_manager()),
            std::string());
}

TEST_F(PasswordInfobarUtilsTest, SignedInWithPasswordsEnabled) {
  SignInWithoutSync();

  std::optional<AccountInfo> account_info =
      GetAccountInfoForPasswordMessages(sync_service(), identity_manager());
  ASSERT_TRUE(account_info.has_value());
  EXPECT_EQ(account_info->email, kTestEmail);
  EXPECT_EQ(GetDisplayableAccountName(sync_service(), identity_manager()),
            kTestEmail);
}

TEST_F(PasswordInfobarUtilsTest, SignedInWithPasswordsDisabled) {
  SignInWithoutSync();
  sync_service()->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kPasswords, false);

  EXPECT_EQ(
      GetAccountInfoForPasswordMessages(sync_service(), identity_manager()),
      std::nullopt);
  EXPECT_EQ(GetDisplayableAccountName(sync_service(), identity_manager()),
            std::string());
}

TEST_F(PasswordInfobarUtilsTest, SyncingWithPasswordsEnabled) {
  SignInWithSync();

  std::optional<AccountInfo> account_info =
      GetAccountInfoForPasswordMessages(sync_service(), identity_manager());
  ASSERT_TRUE(account_info.has_value());
  EXPECT_EQ(account_info->email, kTestEmail);
  EXPECT_EQ(GetDisplayableAccountName(sync_service(), identity_manager()),
            kTestEmail);
}

TEST_F(PasswordInfobarUtilsTest, SyncingWithPasswordsDisabled) {
  SignInWithSync();
  sync_service()->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kPasswords, false);

  EXPECT_EQ(
      GetAccountInfoForPasswordMessages(sync_service(), identity_manager()),
      std::nullopt);
  EXPECT_EQ(GetDisplayableAccountName(sync_service(), identity_manager()),
            std::string());
}

}  // namespace
}  // namespace password_manager
