// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/sync/sync_appsync_optin_client.h"
#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/observer_list.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "components/account_id/account_id.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/base/progress_marker_map.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_observer.h"
#include "components/sync/test/test_sync_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/core_account_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

class FakeSyncService : public syncer::TestSyncService {
 public:
  FakeSyncService() {
    SetMaxTransportState(TransportState::INITIALIZING);
    SetLastCycleSnapshot(syncer::SyncCycleSnapshot());
  }

  FakeSyncService(const FakeSyncService&) = delete;
  FakeSyncService& operator=(const FakeSyncService&) = delete;

  ~FakeSyncService() override { Shutdown(); }

  void SetStatus(bool has_passphrase, bool active) {
    SetMaxTransportState(active ? TransportState::ACTIVE
                                : TransportState::INITIALIZING);
    SetIsUsingExplicitPassphrase(has_passphrase);

    // It doesn't matter what exactly we set here, it's only relevant that the
    // SyncCycleSnapshot is initialized at all.
    SetLastCycleSnapshot(syncer::SyncCycleSnapshot(
        /*birthday=*/std::string(), /*bag_of_chips=*/std::string(),
        syncer::ModelNeutralState(), syncer::ProgressMarkerMap(), false, 0,
        true, base::Time::Now(), base::Time::Now(),
        sync_pb::SyncEnums::UNKNOWN_ORIGIN, base::Minutes(1), false));

    NotifyObserversOfStateChanged();
  }

  void SetAppsyncOptin(bool opted_in) {
    if (opted_in) {
      GetUserSettings()->SetSelectedOsTypes(
          false, {syncer::UserSelectableOsType::kOsApps});
    } else {
      GetUserSettings()->SetSelectedOsTypes(false,
                                            syncer::UserSelectableOsTypeSet());
    }

    NotifyObserversOfStateChanged();
  }

 private:
  void AddObserver(syncer::SyncServiceObserver* observer) override {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(syncer::SyncServiceObserver* observer) override {
    observers_.RemoveObserver(observer);
  }

  void NotifyObserversOfStateChanged() {
    for (auto& observer : observers_) {
      observer.OnStateChanged(this);
    }
  }

  base::ObserverList<syncer::SyncServiceObserver>::Unchecked observers_;
};

}  // namespace

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

 protected:
  void SetUp() override {
    test_user_manager_ = std::make_unique<ash::FakeChromeUserManager>();
    test_sync_service_ = std::make_unique<FakeSyncService>();

    // Take advantage of FakeChromeUserManager not really making hashes
    EXPECT_TRUE(test_daemon_dir_.CreateUniqueTempDir());
    tmp_dir_path_ = test_daemon_dir_.GetPath().Append("test@test.com-hash");
    base::CreateDirectory(tmp_dir_path_);

    auto account_id = AccountId::FromUserEmailGaiaId("test@test.com", "1");
    auto* test_user = RegisterUser(account_id);
    LoginUser(test_user);
    CoreAccountInfo account_info;
    account_info.account_id = CoreAccountId::FromGaiaId(account_id.GetGaiaId());
    account_info.gaia = account_id.GetGaiaId();
    account_info.email = account_id.GetUserEmail();
    test_sync_service_->SetSignedIn(signin::ConsentLevel::kSync, account_info);
    test_sync_service_->SetStatus(/*has_passphrase=*/false, /*active=*/true);
  }

  std::unique_ptr<FakeSyncService> test_sync_service_;
  std::unique_ptr<ash::FakeChromeUserManager> test_user_manager_;
  std::unique_ptr<SyncAppsyncOptinClient> test_appsync_optin_client_;

  base::ScopedTempDir test_daemon_dir_;
  base::FilePath tmp_dir_path_;

  base::test::TaskEnvironment task_environment_;
};

TEST_F(SyncAppsyncOptinClientTest, ServiceCreatesDirectory) {
  EXPECT_TRUE(base::IsDirectoryEmpty(tmp_dir_path_));

  test_appsync_optin_client_ = std::make_unique<SyncAppsyncOptinClient>(
      test_sync_service_.get(), test_user_manager_.get(),
      test_daemon_dir_.GetPath());
  test_sync_service_->SetAppsyncOptin(false);

  // Wait for file IO to finish.
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(base::IsDirectoryEmpty(tmp_dir_path_));
}

TEST_F(SyncAppsyncOptinClientTest, ServiceCreatesOptInFile) {
  EXPECT_TRUE(base::IsDirectoryEmpty(tmp_dir_path_));

  test_sync_service_->SetAppsyncOptin(false);
  test_appsync_optin_client_ = std::make_unique<SyncAppsyncOptinClient>(
      test_sync_service_.get(), test_user_manager_.get(),
      test_daemon_dir_.GetPath());

  // Wait for file IO to finish.
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(base::IsDirectoryEmpty(tmp_dir_path_));
  EXPECT_TRUE(base::PathExists(tmp_dir_path_.Append("opted-in")));
}

TEST_F(SyncAppsyncOptinClientTest, LoggedInUser) {
  test_sync_service_->SetAppsyncOptin(false);
  test_appsync_optin_client_ = std::make_unique<SyncAppsyncOptinClient>(
      test_sync_service_.get(), test_user_manager_.get(),
      test_daemon_dir_.GetPath());

  // Wait for file IO to finish.
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(base::IsDirectoryEmpty(tmp_dir_path_));

  std::string contents;
  EXPECT_TRUE(
      base::ReadFileToString(tmp_dir_path_.Append("opted-in"), &contents));
  EXPECT_EQ("0", contents);
}

TEST_F(SyncAppsyncOptinClientTest, LoggedInUserWithPermission) {
  test_sync_service_->SetAppsyncOptin(true);
  test_appsync_optin_client_ = std::make_unique<SyncAppsyncOptinClient>(
      test_sync_service_.get(), test_user_manager_.get(),
      test_daemon_dir_.GetPath());

  // Wait for file IO to finish.
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(base::IsDirectoryEmpty(tmp_dir_path_));

  std::string contents;
  EXPECT_TRUE(
      base::ReadFileToString(tmp_dir_path_.Append("opted-in"), &contents));
  EXPECT_EQ("1", contents);
}

TEST_F(SyncAppsyncOptinClientTest, UserChangesPermission) {
  test_sync_service_->SetAppsyncOptin(true);
  test_appsync_optin_client_ = std::make_unique<SyncAppsyncOptinClient>(
      test_sync_service_.get(), test_user_manager_.get(),
      test_daemon_dir_.GetPath());

  // Wait for file IO to finish.
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(base::IsDirectoryEmpty(tmp_dir_path_));

  std::string contents;
  EXPECT_TRUE(
      base::ReadFileToString(tmp_dir_path_.Append("opted-in"), &contents));
  EXPECT_EQ("1", contents);

  test_sync_service_->SetAppsyncOptin(false);

  // Wait for file IO to finish.
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(base::IsDirectoryEmpty(tmp_dir_path_));

  EXPECT_TRUE(
      base::ReadFileToString(tmp_dir_path_.Append("opted-in"), &contents));
  EXPECT_EQ("0", contents);
}

TEST_F(SyncAppsyncOptinClientTest, WriteFails) {
  base::HistogramTester histogram_tester;
  test_appsync_optin_client_ = std::make_unique<SyncAppsyncOptinClient>(
      test_sync_service_.get(), test_user_manager_.get(),
      test_daemon_dir_.GetPath().Append("NOT-A-REAL-PATH"));

  // Wait for file IO to finish.
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(base::IsDirectoryEmpty(tmp_dir_path_));
}

TEST_F(SyncAppsyncOptinClientTest, RemovesOldState) {
  // Create old state to migrate
  base::ScopedTempDir old_test_daemon_dir;
  EXPECT_TRUE(old_test_daemon_dir.CreateUniqueTempDir());
  base::FilePath tmp_old_dir_path =
      old_test_daemon_dir.GetPath().Append("test@test.com-hash");
  ASSERT_TRUE(base::CreateDirectory(tmp_old_dir_path));
  ASSERT_TRUE(base::WriteFile(tmp_old_dir_path.Append("consent-enabled"), "1"));

  test_sync_service_->SetAppsyncOptin(true);
  test_appsync_optin_client_ = std::make_unique<SyncAppsyncOptinClient>(
      test_sync_service_.get(), test_user_manager_.get(),
      test_daemon_dir_.GetPath(), old_test_daemon_dir.GetPath());

  // Wait for file IO to finish.
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(base::PathExists(tmp_old_dir_path));
}

TEST_F(SyncAppsyncOptinClientTest, DoenstBreakIfNoOldState) {
  test_sync_service_->SetAppsyncOptin(true);
  test_appsync_optin_client_ = std::make_unique<SyncAppsyncOptinClient>(
      test_sync_service_.get(), test_user_manager_.get(),
      test_daemon_dir_.GetPath(),
      test_daemon_dir_.GetPath().Append("NOT-A-REAL-PATH"));

  EXPECT_TRUE(base::PathExists(tmp_dir_path_));
}

}  // namespace ash
