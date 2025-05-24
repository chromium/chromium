// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/local_files_migration_manager.h"

#include <memory>
#include <optional>

#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_manager/volume_manager_factory.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/policy/skyvault/local_files_migration_constants.h"
#include "chrome/browser/ash/policy/skyvault/migration_notification_manager.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/ash/policy/skyvault/test/skyvault_test_utils.h"
#include "chrome/browser/download/download_dir_util.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/ash/components/dbus/userdataauth/mock_userdataauth_client.h"
#include "chromeos/ash/components/disks/fake_disk_mount_manager.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "profile.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy::local_user_files {

namespace {
constexpr char kTestFile[] = "test_file.txt";
}  // namespace

class LocalFilesMigrationManagerTest : public testing::Test {
 public:
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
        /*enabled_features=*/
        {features::kSkyVault, features::kSkyVaultV2, features::kSkyVaultV3},
        /*disabled_features=*/{});

    scoped_profile_ = std::make_unique<TestingProfile>();
    profile_ = scoped_profile_.get();
    profile_->SetIsNewProfile(true);

    AccountId account_id =
        AccountId::FromUserEmailGaiaId(kEmail, GaiaId("123456"));
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

    // Enable OneDrive.
    profile()->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);
    profile()->GetPrefs()->SetString(prefs::kMicrosoftOneDriveMount,
                                     "automated");

    // By default, VolumeManager null for testing so create one.
    ash::disks::DiskMountManager::InitializeForTesting(
        new ash::disks::FakeDiskMountManager);
    file_manager::VolumeManagerFactory::GetInstance()->SetTestingFactory(
        profile(),
        base::BindLambdaForTesting([](content::BrowserContext* context) {
          return std::unique_ptr<KeyedService>(
              std::make_unique<file_manager::VolumeManager>(
                  Profile::FromBrowserContext(context),
                  /*drive_integration_service=*/nullptr,
                  /*power_manager_client=*/nullptr,
                  ash::disks::DiskMountManager::GetInstance(),
                  /*file_system_provider_service=*/nullptr,
                  file_manager::VolumeManager::GetMtpStorageInfoCallback()));
        }));
  }

  // Creates and registers MyFiles.
  void SetUpMyFiles() {
    my_files_dir_ = GetMyFilesPath(profile());
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(base::CreateDirectory(my_files_dir_));
    }
    std::string mount_point_name =
        file_manager::util::GetDownloadsMountPointName(profile());
    storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
        mount_point_name);
    ASSERT_TRUE(
        storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
            mount_point_name, storage::kFileSystemTypeLocal,
            storage::FileSystemMountOption(), my_files_dir_));
    file_manager::VolumeManager::Get(profile())
        ->RegisterDownloadsDirectoryForTesting(my_files_dir_);
  }

  // Creates a test file in MyFiles.
  base::FilePath CreateTestFile(const std::string& test_file_name) {
    base::FilePath test_file_path;
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      CHECK(base::PathExists(my_files_dir_));
      test_file_path = my_files_dir_.AppendASCII(test_file_name);
      CHECK(base::WriteFile(test_file_path, "42"));
    }
    return test_file_path;
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

  // Sets the migration state, local user files allowed and migration
  // destination prefs to the provided values. By default, disables local
  // storage and enables migration to Microsoft OneDrive.
  void SetPrefs(
      State state,
      bool local_user_files_allowed = false,
      const std::string& destination = download_dir_util::kLocationOneDrive) {
    SetLocalUserFilesAllowed(local_user_files_allowed);
    scoped_testing_local_state_.Get()->SetString(
        prefs::kLocalUserFilesMigrationDestination, destination);

    profile()->GetPrefs()->SetInteger(prefs::kSkyVaultMigrationState,
                                      static_cast<int>(state));
  }

  // Sets the local user files allowed pref value.
  void SetLocalUserFilesAllowed(bool local_user_files_allowed) {
    scoped_testing_local_state_.Get()->SetBoolean(prefs::kLocalUserFilesAllowed,
                                                  local_user_files_allowed);
  }

  // Sets the local user files migration destination pref value.
  void SetMigrationDestination(const std::string& destination) {
    scoped_testing_local_state_.Get()->SetString(
        prefs::kLocalUserFilesMigrationDestination, destination);
  }

  void SetRetryCount(int count) {
    profile()->GetPrefs()->SetInteger(prefs::kSkyVaultMigrationRetryCount,
                                      count);
  }

  base::HistogramTester histogram_tester_;
  testing::NiceMock<ash::MockUserDataAuthClient> userdataauth_;

 private:
  ScopedTestingLocalState scoped_testing_local_state_;
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  ash::system::FakeStatisticsProvider statistics_provider_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  std::unique_ptr<TestingProfile> scoped_profile_;
  raw_ptr<TestingProfile> profile_;
  base::FilePath my_files_dir_;
};

class LocalFilesMigrationManagerStateTest
    : public LocalFilesMigrationManagerTest,
      public ::testing::WithParamInterface<
          std::tuple<State,
                     /*expected_dialog*/ int,
                     /*expected_run_count*/ int>> {
 public:
  static std::string ParamToName(const testing::TestParamInfo<ParamType> info) {
    State state = std::get<0>(info.param);
    switch (state) {
      case State::kUninitialized:
        return "uninitialized";
      case State::kPending:
        return "pending";
      case State::kInProgress:
        return "in_progress";
      case State::kCleanup:
        return "cleanup";
      case State::kCompleted:
        return "completed";
      case State::kFailure:
        return "failure";
    }
  }

  LocalFilesMigrationManagerStateTest() = default;
  LocalFilesMigrationManagerStateTest(
      const LocalFilesMigrationManagerStateTest&) = delete;
  LocalFilesMigrationManagerStateTest& operator=(
      const LocalFilesMigrationManagerStateTest&) = delete;
  ~LocalFilesMigrationManagerStateTest() override = default;
};

TEST_F(LocalFilesMigrationManagerTest, ResetStateIfLocalStorageAllowed) {
  SetUpMyFiles();
  CreateTestFile(kTestFile);
  SetPrefs(State::kPending,
           /*local_user_files_allowed=*/true);

  LocalFilesMigrationManager manager(profile());
  manager.Initialize();
  histogram_tester_.ExpectBucketCount("Enterprise.SkyVault.Migration.Reset",
                                      true, 1);
}

TEST_F(LocalFilesMigrationManagerTest, ResetStateIfMigrationDisabled) {
  SetUpMyFiles();
  CreateTestFile(kTestFile);
  SetPrefs(State::kInProgress,
           /*local_user_files_allowed=*/false, "read_only");

  LocalFilesMigrationManager manager(profile());
  manager.Initialize();
  histogram_tester_.ExpectBucketCount("Enterprise.SkyVault.Migration.Reset",
                                      true, 1);
}

TEST_F(LocalFilesMigrationManagerTest, NoResetStateIfAlreadyDisabled) {
  SetUpMyFiles();
  CreateTestFile(kTestFile);
  SetPrefs(State::kUninitialized,
           /*local_user_files_allowed=*/false, "read_only");

  LocalFilesMigrationManager manager(profile());
  manager.Initialize();
  histogram_tester_.ExpectBucketCount("Enterprise.SkyVault.Migration.Reset",
                                      true, 0);
}

TEST_F(LocalFilesMigrationManagerTest, HandlesMigrationFailures) {
  SetUpMyFiles();
  base::FilePath test_file_path = CreateTestFile(kTestFile);
  SetPrefs(State::kInProgress);
  SetRetryCount(kMaxRetryCount);

  std::unique_ptr<MockMigrationCoordinator> coordinator =
      std::make_unique<MockMigrationCoordinator>(profile());
  base::test::TestFuture<void> run_future;
  MockMigrationCoordinator* coordinator_ptr = coordinator.get();
  EXPECT_CALL(*coordinator_ptr, Run)
      .WillOnce([&test_file_path, &run_future](
                    MigrationDestination destination,
                    std::vector<base::FilePath> file_paths,
                    const std::string& upload_root,
                    MigrationDoneCallback callback) {
        std::move(callback).Run(
            {
                {test_file_path, MigrationUploadError::kCopyFailed},
            },
            base::FilePath(),
            base::FilePath(kErrorLogFileBasePath).Append(kErrorLogFileName));
        run_future.SetValue();
      });

  std::unique_ptr<MockMigrationNotificationManager> notification_manager =
      std::make_unique<MockMigrationNotificationManager>(profile());

  LocalFilesMigrationManager manager(profile());
  manager.SetNotificationManagerForTesting(notification_manager.get());
  manager.SetCoordinatorForTesting(std::move(coordinator));
  manager.Initialize();
  ASSERT_TRUE(run_future.Wait());

  histogram_tester_.ExpectBucketCount(
      "Enterprise.SkyVault.Migration.OneDrive.Failed", true, 1);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.SkyVault.Migration.OneDrive.FailureDuration", 1);
}

TEST_F(LocalFilesMigrationManagerTest, RetriesIfAllowed) {
  SetUpMyFiles();
  base::FilePath test_file_path = CreateTestFile(kTestFile);
  SetPrefs(State::kInProgress);
  SetRetryCount(2);

  std::unique_ptr<MockMigrationCoordinator> coordinator =
      std::make_unique<MockMigrationCoordinator>(profile());
  base::test::TestFuture<void> run_future;
  MockMigrationCoordinator* coordinator_ptr = coordinator.get();
  EXPECT_CALL(*coordinator_ptr, Run)
      .WillOnce(
          [&test_file_path, &run_future](MigrationDestination destination,
                                         std::vector<base::FilePath> file_paths,
                                         const std::string& upload_root,
                                         MigrationDoneCallback callback) {
            std::move(callback).Run(
                {
                    {test_file_path, MigrationUploadError::kCopyFailed},
                },
                base::FilePath(), base::FilePath());
            run_future.SetValue();
          })
      .WillOnce([&run_future](MigrationDestination destination,
                              std::vector<base::FilePath> file_paths,
                              const std::string& upload_root,
                              MigrationDoneCallback callback) {
        std::move(callback).Run({}, base::FilePath(), base::FilePath());
        run_future.SetValue();
      });

  std::unique_ptr<MockMigrationNotificationManager> notification_manager =
      std::make_unique<MockMigrationNotificationManager>(profile());

  LocalFilesMigrationManager manager(profile());
  manager.SetNotificationManagerForTesting(notification_manager.get());
  manager.SetCoordinatorForTesting(std::move(coordinator));
  manager.Initialize();
  ASSERT_TRUE(run_future.WaitAndClear());

  histogram_tester_.ExpectUniqueSample("Enterprise.SkyVault.Migration.Retry",
                                       /*sample=*/3.0,
                                       /*expected_bucket_count=*/1);
  ASSERT_TRUE(run_future.Wait());
}

TEST_F(LocalFilesMigrationManagerTest, DoesNotRetryWhenFatal) {
  SetUpMyFiles();
  base::FilePath test_file_path = CreateTestFile(kTestFile);
  SetPrefs(State::kInProgress);
  SetRetryCount(2);

  std::unique_ptr<MockMigrationCoordinator> coordinator =
      std::make_unique<MockMigrationCoordinator>(profile());
  base::test::TestFuture<void> run_future;
  MockMigrationCoordinator* coordinator_ptr = coordinator.get();
  EXPECT_CALL(*coordinator_ptr, Run)
      .WillOnce([&test_file_path, &run_future](
                    MigrationDestination destination,
                    std::vector<base::FilePath> file_paths,
                    const std::string& upload_root,
                    MigrationDoneCallback callback) {
        std::move(callback).Run(
            {
                {test_file_path, MigrationUploadError::kCloudQuotaFull},
            },
            base::FilePath(),
            base::FilePath(kErrorLogFileBasePath).Append(kErrorLogFileName));
        run_future.SetValue();
      });

  std::unique_ptr<MockMigrationNotificationManager> notification_manager =
      std::make_unique<MockMigrationNotificationManager>(profile());

  LocalFilesMigrationManager manager(profile());
  manager.SetNotificationManagerForTesting(notification_manager.get());
  manager.SetCoordinatorForTesting(std::move(coordinator));
  manager.Initialize();
  ASSERT_TRUE(run_future.Wait());

  histogram_tester_.ExpectBucketCount(
      "Enterprise.SkyVault.Migration.OneDrive.Failed", true, 1);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.SkyVault.Migration.OneDrive.FailureDuration", 1);
}

TEST_F(LocalFilesMigrationManagerTest, HandlesWriteAccessError) {
  SetUpMyFiles();
  CreateTestFile(kTestFile);
  EXPECT_CALL(userdataauth_,
              SetUserDataStorageWriteEnabled(WithEnabled(false), testing::_))
      .Times(1)
      .WillRepeatedly(ReplyWith(std::nullopt));

  SetPrefs(State::kCleanup);

  LocalFilesMigrationManager manager(profile());
  manager.Initialize();
  // Wait for async functions to complete.
  base::RunLoop().RunUntilIdle();
  histogram_tester_.ExpectBucketCount(
      "Enterprise.SkyVault.Migration.WriteAccessError", true, 1);
}

TEST_F(LocalFilesMigrationManagerTest, UsesExistingStartTimeFromPrefs) {
  SetUpMyFiles();
  CreateTestFile(kTestFile);

  base::Time start_time = base::Time::Now() + base::Hours(5);
  profile()->GetPrefs()->SetTime(prefs::kSkyVaultMigrationScheduledStartTime,
                                 start_time);
  std::unique_ptr<MockMigrationNotificationManager> notification_manager =
      std::make_unique<MockMigrationNotificationManager>(profile());
  EXPECT_CALL(*notification_manager.get(),
              ShowMigrationInfoDialog(MigrationDestination::kOneDrive,
                                      start_time, testing::_))
      .Times(1);

  SetPrefs(State::kPending);

  LocalFilesMigrationManager manager(profile());
  manager.SetNotificationManagerForTesting(notification_manager.get());

  manager.Initialize();
  // Wait for async functions to complete.
  base::RunLoop().RunUntilIdle();
}

TEST_F(LocalFilesMigrationManagerTest, InformUserShortTimeJumpsToSecond) {
  SetUpMyFiles();
  CreateTestFile(kTestFile);

  base::Time start_time = base::Time::Now() + base::Minutes(34);
  profile()->GetPrefs()->SetTime(prefs::kSkyVaultMigrationScheduledStartTime,
                                 start_time);
  std::unique_ptr<MockMigrationNotificationManager> notification_manager =
      std::make_unique<MockMigrationNotificationManager>(profile());
  EXPECT_CALL(*notification_manager.get(),
              ShowMigrationInfoDialog(MigrationDestination::kOneDrive,
                                      start_time, testing::_))
      .Times(1);

  SetPrefs(State::kPending);

  LocalFilesMigrationManager manager(profile());
  manager.SetNotificationManagerForTesting(notification_manager.get());

  manager.Initialize();
  // Wait for async functions to complete.
  base::RunLoop().RunUntilIdle();
}

TEST_F(LocalFilesMigrationManagerTest, StoresScheduledTimeToPrefs) {
  SetUpMyFiles();
  CreateTestFile(kTestFile);

  std::unique_ptr<MockMigrationNotificationManager> notification_manager =
      std::make_unique<MockMigrationNotificationManager>(profile());
  EXPECT_CALL(*notification_manager.get(), ShowMigrationInfoDialog).Times(1);

  SetPrefs(State::kPending);

  LocalFilesMigrationManager manager(profile());
  manager.SetNotificationManagerForTesting(notification_manager.get());

  manager.Initialize();
  // Wait for async functions to complete.
  base::RunLoop().RunUntilIdle();

  base::Time start_time = profile()->GetPrefs()->GetTime(
      prefs::kSkyVaultMigrationScheduledStartTime);
  EXPECT_FALSE(start_time.is_null());
}

TEST_F(LocalFilesMigrationManagerTest, StartsNowIfStartTimePast) {
  SetUpMyFiles();
  CreateTestFile(kTestFile);

  base::Time start_time = base::Time::Now() - base::Hours(5);
  profile()->GetPrefs()->SetTime(prefs::kSkyVaultMigrationScheduledStartTime,
                                 start_time);
  std::unique_ptr<MockMigrationNotificationManager> notification_manager =
      std::make_unique<MockMigrationNotificationManager>(profile());
  EXPECT_CALL(*notification_manager.get(), ShowMigrationInfoDialog).Times(0);

  SetPrefs(State::kPending, /*local_user_files_allowed=*/false,
           /*destination=*/"delete");

  LocalFilesMigrationManager manager(profile());
  manager.SetNotificationManagerForTesting(notification_manager.get());

  manager.Initialize();
  // Wait for async functions to complete.
  base::RunLoop().RunUntilIdle();

  histogram_tester_.ExpectBucketCount(
      "Enterprise.SkyVault.Migration.Delete.ScheduledTimeInPast.InformUser",
      true, 1);
}

TEST_F(LocalFilesMigrationManagerTest, StopsWhenLocalStorageAllowed) {
  SetUpMyFiles();
  CreateTestFile(kTestFile);
  EXPECT_CALL(userdataauth_,
              SetUserDataStorageWriteEnabled(WithEnabled(true), testing::_))
      .Times(1)
      .WillRepeatedly(
          ReplyWith(::user_data_auth::SetUserDataStorageWriteEnabledReply()));

  std::unique_ptr<MockMigrationNotificationManager> notification_manager =
      std::make_unique<MockMigrationNotificationManager>(profile());
  std::unique_ptr<MockMigrationCoordinator> coordinator =
      std::make_unique<MockMigrationCoordinator>(profile());
  base::test::TestFuture<void> run_future;
  coordinator->SetRunCallback(run_future.GetRepeatingCallback());
  MockMigrationCoordinator* coordinator_ptr = coordinator.get();
  EXPECT_CALL(*coordinator_ptr, Run).Times(1);
  EXPECT_CALL(*coordinator_ptr, Cancel).Times(1);

  SetPrefs(State::kInProgress);

  LocalFilesMigrationManager manager(profile());
  manager.SetNotificationManagerForTesting(notification_manager.get());
  manager.SetCoordinatorForTesting(std::move(coordinator));
  manager.Initialize();

  // Wait for Run as it's async.
  ASSERT_TRUE(run_future.Wait());
  histogram_tester_.ExpectBucketCount(
      "Enterprise.SkyVault.LocalStorage.Enabled", false, 1);
  histogram_tester_.ExpectBucketCount(
      "Enterprise.SkyVault.Migration.OneDrive.Enabled", true, 1);

  SetLocalUserFilesAllowed(true);

  histogram_tester_.ExpectBucketCount(
      "Enterprise.SkyVault.LocalStorage.Enabled", true, 1);
  histogram_tester_.ExpectBucketCount(
      "Enterprise.SkyVault.Migration.OneDrive.Stopped", true, 1);
}

TEST_F(LocalFilesMigrationManagerTest, DoesNotRetryDeleteIndefinitely) {
  SetUpMyFiles();
  base::FilePath test_file_path = CreateTestFile(kTestFile);
  SetPrefs(State::kCleanup, /*local_user_files_allowed=*/false,
           /*destination=*/"delete");
  SetRetryCount(kMaxRetryCount);

  std::unique_ptr<MockCleanupHandler> cleanup_handler =
      std::make_unique<MockCleanupHandler>();
  EXPECT_CALL(*cleanup_handler, Cleanup)
      .WillOnce(
          [](base::OnceCallback<void(
                 const std::optional<std::string>& error_message)> callback) {
            std::move(callback).Run("Something failed");
          });

  LocalFilesMigrationManager manager(profile());
  manager.SetCleanupHandlerForTesting(cleanup_handler->GetWeakPtr());
  manager.Initialize();

  // Wait for async functions, like checking if MyFiles is empty, to complete.
  base::RunLoop().RunUntilIdle();

  histogram_tester_.ExpectBucketCount(
      "Enterprise.SkyVault.Migration.Delete.CleanupError", true, 1);
  histogram_tester_.ExpectBucketCount(
      "Enterprise.SkyVault.Migration.Delete.Failed", true, 1);
}

TEST_F(LocalFilesMigrationManagerTest, RetriesDeleteOnSessionStart) {
  SetUpMyFiles();
  base::FilePath test_file_path = CreateTestFile(kTestFile);
  SetPrefs(State::kFailure, /*local_user_files_allowed=*/false,
           /*destination=*/"delete");
  SetRetryCount(kMaxRetryCount);

  std::unique_ptr<MockMigrationNotificationManager> notification_manager =
      std::make_unique<MockMigrationNotificationManager>(profile());
  EXPECT_CALL(*notification_manager, ShowDeletionCompletedNotification)
      .Times(1);
  std::unique_ptr<MockCleanupHandler> cleanup_handler =
      std::make_unique<MockCleanupHandler>();
  EXPECT_CALL(*cleanup_handler, Cleanup);

  LocalFilesMigrationManager manager(profile());
  manager.SetNotificationManagerForTesting(notification_manager.get());
  manager.SetCleanupHandlerForTesting(cleanup_handler->GetWeakPtr());
  manager.Initialize();

  // Wait for async functions, like checking if MyFiles is empty, to complete.
  base::RunLoop().RunUntilIdle();

  // The retry count should be reset.
  int retry_count =
      profile()->GetPrefs()->GetInteger(prefs::kSkyVaultMigrationRetryCount);
  EXPECT_EQ(0, retry_count);

  histogram_tester_.ExpectBucketCount(
      "Enterprise.SkyVault.Migration.Delete.Failed", false, 1);
}

TEST_P(LocalFilesMigrationManagerStateTest, InitializeFromState) {
  SetUpMyFiles();
  CreateTestFile(kTestFile);
  auto [state, expected_dialog_count, expected_run_count] = GetParam();
  SetPrefs(state);

  std::unique_ptr<MockMigrationNotificationManager> notification_manager =
      std::make_unique<MockMigrationNotificationManager>(profile());
  std::unique_ptr<MockMigrationCoordinator> coordinator =
      std::make_unique<MockMigrationCoordinator>(profile());
  base::test::TestFuture<void> run_future;
  coordinator->SetRunCallback(run_future.GetRepeatingCallback());
  MockMigrationCoordinator* coordinator_ptr = coordinator.get();

  EXPECT_CALL(*notification_manager.get(), ShowMigrationInfoDialog)
      .Times(expected_dialog_count);
  EXPECT_CALL(*coordinator_ptr, Run).Times(expected_run_count);

  LocalFilesMigrationManager manager(profile());
  manager.SetNotificationManagerForTesting(notification_manager.get());
  manager.SetCoordinatorForTesting(std::move(coordinator));
  manager.Initialize();

  // Wait for async functions, like checking if MyFiles is empty, to complete.
  base::RunLoop().RunUntilIdle();

  if (expected_run_count) {
    // Wait for Run as it's async.
    ASSERT_TRUE(run_future.Wait());
  }
}

INSTANTIATE_TEST_SUITE_P(
    LocalFilesMigration,
    LocalFilesMigrationManagerStateTest,
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
    LocalFilesMigrationManagerStateTest::ParamToName);

}  // namespace policy::local_user_files
