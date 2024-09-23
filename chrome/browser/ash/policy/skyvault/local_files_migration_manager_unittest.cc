// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/local_files_migration_manager.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/policy/skyvault/migration_notification_manager.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/ash/policy/skyvault/test/skyvault_test_utils.h"
#include "chrome/browser/download/download_dir_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/ash/components/dbus/userdataauth/mock_userdataauth_client.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy::local_user_files {

using State = LocalFilesMigrationManager::State;

class LocalFilesMigrationManagerTest
    : public testing::Test,
      public ::testing::WithParamInterface<
          std::tuple<State,
                     /*expected_dialog*/ int,
                     /*expected_run_count*/ int>> {
 public:
  static std::string ParamToName(const testing::TestParamInfo<ParamType> info) {
    State state = std::get<0>(info.param);
    switch (state) {
      case LocalFilesMigrationManager::State::kUninitialized:
        return "uninitialized";
      case LocalFilesMigrationManager::State::kPending:
        return "pending";
      case LocalFilesMigrationManager::State::kInProgress:
        return "in_progress";
      case LocalFilesMigrationManager::State::kCleanup:
        return "cleanup";
      case LocalFilesMigrationManager::State::kCompleted:
        return "completed";
      case LocalFilesMigrationManager::State::kFailure:
        return "failure";
    }
  }

  LocalFilesMigrationManagerTest()
      : scoped_testing_local_state_(TestingBrowserProcess::GetGlobal()) {}

  LocalFilesMigrationManagerTest(const LocalFilesMigrationManagerTest&) =
      delete;
  LocalFilesMigrationManagerTest& operator=(
      const LocalFilesMigrationManagerTest&) = delete;

  ~LocalFilesMigrationManagerTest() override = default;

 protected:
  void SetUp() override {
    testing::Test::SetUp();
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kSkyVault, features::kSkyVaultV2},
        /*disabled_features=*/{});

    scoped_profile_ = std::make_unique<TestingProfile>();
    profile_ = scoped_profile_.get();
    profile_->SetIsNewProfile(true);

    AccountId account_id = AccountId::FromUserEmailGaiaId(kEmail, "123456");
    ash::AnnotatedAccountId::Set(profile_, account_id);

    auto user_manager = std::make_unique<ash::FakeChromeUserManager>();
    user_manager->AddUserWithAffiliationAndTypeAndProfile(
        account_id, /*is_affiliated=*/false, user_manager::UserType::kRegular,
        profile_);
    user_manager->LoginUser(account_id,
                            /*set_profile_created_flag=*/true);
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));

    statistics_provider_.SetMachineStatistic(ash::system::kSerialNumberKey,
                                             "12345689");
    ash::system::StatisticsProvider::SetTestProvider(&statistics_provider_);

    ash::UserDataAuthClient::OverrideGlobalInstanceForTesting(&userdataauth_);
  }

  void TearDown() override {
    ash::UserDataAuthClient::OverrideGlobalInstanceForTesting(
        ash::FakeUserDataAuthClient::Get());
    profile_ = nullptr;
    scoped_user_manager_.reset();
    scoped_profile_.reset();
    testing::Test::TearDown();
  }

  Profile* profile() { return profile_; }

  // Disables local storage and enables migration, and sets the migration state
  // pref to the provided value.
  void SetPrefs(State state) {
    scoped_testing_local_state_.Get()->SetString(
        prefs::kLocalUserFilesMigrationDestination,
        download_dir_util::kLocationGoogleDrive);
    scoped_testing_local_state_.Get()->SetBoolean(prefs::kLocalUserFilesAllowed,
                                                  false);

    profile()->GetPrefs()->SetInteger(prefs::kSkyVaultMigrationState,
                                      static_cast<int>(state));
  }

 private:
  ScopedTestingLocalState scoped_testing_local_state_;
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  ash::system::FakeStatisticsProvider statistics_provider_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  std::unique_ptr<TestingProfile> scoped_profile_;
  raw_ptr<TestingProfile> profile_;
  testing::NiceMock<ash::MockUserDataAuthClient> userdataauth_;
};

TEST_P(LocalFilesMigrationManagerTest, InitializeFromState) {
  auto [state, expected_dialog_count, expected_run_count] = GetParam();
  SetPrefs(state);

  std::unique_ptr<MockMigrationNotificationManager> notification_manager =
      std::make_unique<MockMigrationNotificationManager>(profile());
  std::unique_ptr<MockMigrationCoordinator> coordinator =
      std::make_unique<MockMigrationCoordinator>(profile());
  MockMigrationCoordinator* coordinator_ptr = coordinator.get();

  EXPECT_CALL(*notification_manager.get(), ShowMigrationInfoDialog)
      .Times(expected_dialog_count);
  EXPECT_CALL(*coordinator_ptr, Run).Times(expected_run_count);

  LocalFilesMigrationManager manager(profile());
  manager.SetNotificationManagerForTesting(notification_manager.get());
  manager.SetCoordinatorForTesting(std::move(coordinator));
  manager.Initialize();

  // Needed to wait for Run in case of InProgress state.
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

INSTANTIATE_TEST_SUITE_P(
    LocalFilesMigration,
    LocalFilesMigrationManagerTest,
    ::testing::Values(std::make_tuple(State::kUninitialized,
                                      /*expected_dialog_count*/ 1,
                                      /*expected_run_count*/ 0),
                      std::make_tuple(State::kPending,
                                      /*expected_dialog_count*/ 1,
                                      /*expected_run_count*/ 0),
                      std::make_tuple(State::kInProgress,
                                      /*expected_dialog_count*/ 0,
                                      /*expected_run_count*/ 1),
                      std::make_tuple(State::kFailure,
                                      /*expected_dialog_count*/ 0,
                                      /*expected_run_count*/ 0),
                      std::make_tuple(State::kCleanup,
                                      /*expected_dialog_count*/ 0,
                                      /*expected_run_count*/ 0),
                      std::make_tuple(State::kCompleted,
                                      /*expected_dialog_count*/ 0,
                                      /*expected_run_count*/ 0)),
    LocalFilesMigrationManagerTest::ParamToName);

}  // namespace policy::local_user_files
