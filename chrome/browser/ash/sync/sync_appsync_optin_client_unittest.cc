// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/sync/sync_appsync_optin_client.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "components/account_id/account_id.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/test/test_sync_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

class SyncAppsyncOptinClientTest : public testing::Test {
 public:
  SyncAppsyncOptinClientTest() = default;

  SyncAppsyncOptinClientTest(const SyncAppsyncOptinClientTest&) = delete;
  SyncAppsyncOptinClientTest& operator=(const SyncAppsyncOptinClientTest&) =
      delete;

  ~SyncAppsyncOptinClientTest() override {
    test_appsync_optin_client_.reset();
    test_user_manager_.reset();
  }

  user_manager::User* RegisterUser(const AccountId& account_id) {
    return test_user_manager_->AddUser(account_id);
  }

  void LoginUser(user_manager::User* user) {
    test_user_manager_->LoginUser(user->GetAccountId());
    test_user_manager_->SwitchActiveUser(user->GetAccountId());
    test_user_manager_->SimulateUserProfileLoad(user->GetAccountId());
  }

  void SetAppsyncOptin(bool opted_in) {
    sync_service_.GetUserSettings()->SetSelectedOsTypes(
        false,
        opted_in
            ? syncer::
                  UserSelectableOsTypeSet{syncer::UserSelectableOsType::kOsApps}
            : syncer::UserSelectableOsTypeSet());
    sync_service_.FireStateChanged();
  }

 protected:
  void SetUp() override {
    test_user_manager_ = std::make_unique<ash::FakeChromeUserManager>();

    // Take advantage of FakeChromeUserManager not really making hashes
    EXPECT_TRUE(test_daemon_dir_.CreateUniqueTempDir());
    tmp_dir_path_ = test_daemon_dir_.GetPath().Append("test@test.com-hash");
    base::CreateDirectory(tmp_dir_path_);

    auto account_id =
        AccountId::FromUserEmailGaiaId("test@test.com", GaiaId("1"));
    auto* test_user = RegisterUser(account_id);
    LoginUser(test_user);
    CoreAccountInfo account_info;
    account_info.account_id = CoreAccountId::FromGaiaId(account_id.GetGaiaId());
    account_info.gaia = account_id.GetGaiaId();
    account_info.email = account_id.GetUserEmail();
    sync_service_.SetSignedIn(signin::ConsentLevel::kSync, account_info);
  }

  base::test::TaskEnvironment task_environment_;

  syncer::TestSyncService sync_service_;
  std::unique_ptr<ash::FakeChromeUserManager> test_user_manager_;
  std::unique_ptr<SyncAppsyncOptinClient> test_appsync_optin_client_;

  base::ScopedTempDir test_daemon_dir_;
  base::FilePath tmp_dir_path_;
};

TEST_F(SyncAppsyncOptinClientTest, ServiceCreatesDirectory) {
  EXPECT_TRUE(base::IsDirectoryEmpty(tmp_dir_path_));

  test_appsync_optin_client_ = std::make_unique<SyncAppsyncOptinClient>(
      &sync_service_, test_user_manager_.get(), test_daemon_dir_.GetPath());
  SetAppsyncOptin(false);

  // Wait for file IO to finish.
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(base::IsDirectoryEmpty(tmp_dir_path_));
}

TEST_F(SyncAppsyncOptinClientTest, ServiceCreatesOptInFile) {
  EXPECT_TRUE(base::IsDirectoryEmpty(tmp_dir_path_));

  SetAppsyncOptin(false);
  test_appsync_optin_client_ = std::make_unique<SyncAppsyncOptinClient>(
      &sync_service_, test_user_manager_.get(), test_daemon_dir_.GetPath());

  // Wait for file IO to finish.
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(base::IsDirectoryEmpty(tmp_dir_path_));
  EXPECT_TRUE(base::PathExists(tmp_dir_path_.Append("opted-in")));
}

TEST_F(SyncAppsyncOptinClientTest, LoggedInUser) {
  SetAppsyncOptin(false);
  test_appsync_optin_client_ = std::make_unique<SyncAppsyncOptinClient>(
      &sync_service_, test_user_manager_.get(), test_daemon_dir_.GetPath());

  // Wait for file IO to finish.
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(base::IsDirectoryEmpty(tmp_dir_path_));

  std::string contents;
  EXPECT_TRUE(
      base::ReadFileToString(tmp_dir_path_.Append("opted-in"), &contents));
  EXPECT_EQ("0", contents);
}

TEST_F(SyncAppsyncOptinClientTest, LoggedInUserWithPermission) {
  SetAppsyncOptin(true);
  test_appsync_optin_client_ = std::make_unique<SyncAppsyncOptinClient>(
      &sync_service_, test_user_manager_.get(), test_daemon_dir_.GetPath());

  // Wait for file IO to finish.
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(base::IsDirectoryEmpty(tmp_dir_path_));

  std::string contents;
  EXPECT_TRUE(
      base::ReadFileToString(tmp_dir_path_.Append("opted-in"), &contents));
  EXPECT_EQ("1", contents);
}

TEST_F(SyncAppsyncOptinClientTest, UserChangesPermission) {
  SetAppsyncOptin(true);
  test_appsync_optin_client_ = std::make_unique<SyncAppsyncOptinClient>(
      &sync_service_, test_user_manager_.get(), test_daemon_dir_.GetPath());

  // Wait for file IO to finish.
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(base::IsDirectoryEmpty(tmp_dir_path_));

  std::string contents;
  EXPECT_TRUE(
      base::ReadFileToString(tmp_dir_path_.Append("opted-in"), &contents));
  EXPECT_EQ("1", contents);

  SetAppsyncOptin(false);

  // Wait for file IO to finish.
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(base::IsDirectoryEmpty(tmp_dir_path_));

  EXPECT_TRUE(
      base::ReadFileToString(tmp_dir_path_.Append("opted-in"), &contents));
  EXPECT_EQ("0", contents);
}

TEST_F(SyncAppsyncOptinClientTest, WriteFails) {
  test_appsync_optin_client_ = std::make_unique<SyncAppsyncOptinClient>(
      &sync_service_, test_user_manager_.get(),
      test_daemon_dir_.GetPath().Append("NOT-A-REAL-PATH"));

  // Wait for file IO to finish.
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(base::IsDirectoryEmpty(tmp_dir_path_));
}

}  // namespace
}  // namespace ash
