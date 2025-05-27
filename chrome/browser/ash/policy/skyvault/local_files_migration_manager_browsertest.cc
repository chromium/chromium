// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/local_files_migration_manager.h"

#include <memory>
#include <optional>
#include <string>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/drive_integration_service_factory.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/policy/skyvault/local_files_migration_constants.h"
#include "chrome/browser/ash/policy/skyvault/migration_coordinator.h"
#include "chrome/browser/ash/policy/skyvault/migration_notification_manager.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/ash/policy/skyvault/test/skyvault_test_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/files_cleanup_handler.h"
#include "chrome/browser/download/download_dir_util.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/ash/components/dbus/userdataauth/mock_userdataauth_client.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace policy::local_user_files {

namespace {
constexpr char kReadOnly[] = "read_only";

constexpr char kTestDeviceSerialNumber[] = "12345689";

constexpr base::TimeDelta kMaxDelta = base::Seconds(1);

constexpr char kMigrationEnabledUMASuffix[] = "Enabled";
constexpr char kMigrationMisconfiguredUMASuffix[] = "Misconfigured";
constexpr char kMigrationFailedUMASuffix[] = "Failed";
constexpr char kMigrationSuccessDurationUMASuffix[] = "SuccessDuration";

constexpr char kTestFile[] = "test_file.txt";

// Matcher for scheduled migration time.
MATCHER_P(TimeNear, expected_time, "") {
  base::TimeDelta delta = (arg - expected_time).magnitude();
  return delta <= kMaxDelta;
}

// Constructs the expected directory name on the cloud.
std::string ExpectedUploadRootName() {
  return std::string(kUploadRootPrefix) + " " +
         std::string(kTestDeviceSerialNumber);
}

std::string GetUMAName(const std::string& destination,
                       const std::string& suffix) {
  const std::string provider =
      (destination == download_dir_util::kLocationGoogleDrive) ? "GoogleDrive"
                                                               : "OneDrive";
  return base::StrCat(
      {"Enterprise.SkyVault.Migration.", provider, ".", suffix});
}

MigrationDestination GetCloudProvider(const std::string& destination) {
  if (destination == download_dir_util::kLocationGoogleDrive) {
    return MigrationDestination::kGoogleDrive;
  }
  if (destination == download_dir_util::kLocationOneDrive) {
    return MigrationDestination::kOneDrive;
  }
  return MigrationDestination::kNotSpecified;
}
}  // namespace

class LocalFilesMigrationManagerTest : public policy::PolicyTest {
 public:
  LocalFilesMigrationManagerTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kSkyVault, features::kSkyVaultV2,
                              features::kSkyVaultV3,
                              chromeos::features::kUploadOfficeToCloud},
        /*disabled_features=*/{});
  }
  ~LocalFilesMigrationManagerTest() override = default;

  void SetUpOnMainThread() override {
    policy::PolicyTest::SetUpOnMainThread();

    browser()
        ->profile()
        ->GetProfilePolicyConnector()
        ->OverrideIsManagedForTesting(true);
    SetOneDrivePolicy("allowed");

    statistics_provider_.SetMachineStatistic(ash::system::kSerialNumberKey,
                                             kTestDeviceSerialNumber);
    ash::system::StatisticsProvider::SetTestProvider(&statistics_provider_);

    ASSERT_TRUE(manager());

    manager()->AddObserver(&observer_);

    notification_manager_ = std::make_unique<MockMigrationNotificationManager>(
        browser()->profile());
    manager()->SetNotificationManagerForTesting(notification_manager_.get());

    ash::UserDataAuthClient::OverrideGlobalInstanceForTesting(&userdataauth_);
  }

  void TearDownOnMainThread() override {
    ash::UserDataAuthClient::OverrideGlobalInstanceForTesting(
        ash::FakeUserDataAuthClient::Get());

    manager()->SetNotificationManagerForTesting(
        MigrationNotificationManagerFactory::GetInstance()
            ->GetForBrowserContext(browser()->profile()));
    notification_manager_.reset();

    policy::PolicyTest::TearDownOnMainThread();
  }

 protected:
  void SetMigrationPolicies(bool local_user_files_allowed,
                            const std::string& destination) {
    policy::PolicyMap policies;
    policy::PolicyTest::SetPolicy(&policies,
                                  policy::key::kLocalUserFilesAllowed,
                                  base::Value(local_user_files_allowed));
    policy::PolicyTest::SetPolicy(
        &policies, policy::key::kLocalUserFilesMigrationDestination,
        base::Value(destination));
    provider_.UpdateChromePolicy(policies);
  }

  // Sets the value of MicrosoftOneDriveMount policy to `mount`, which should be
  // one of 'allowed', 'automated', 'disallowed'.
  void SetOneDrivePolicy(const std::string& mount) {
    policy::PolicyMap policies;
    policy::PolicyTest::SetPolicy(
        &policies, policy::key::kMicrosoftOneDriveMount, base::Value(mount));
    provider_.UpdateChromePolicy(policies);
  }

  // Creates mount point for My files and registers local filesystem.
  void SetUpMyFiles() {
    my_files_dir_ = GetMyFilesPath(browser()->profile());
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(base::CreateDirectory(my_files_dir_));
    }
    std::string mount_point_name =
        file_manager::util::GetDownloadsMountPointName(browser()->profile());
    storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
        mount_point_name);
    CHECK(storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
        mount_point_name, storage::kFileSystemTypeLocal,
        storage::FileSystemMountOption(), my_files_dir_));
    file_manager::VolumeManager::Get(browser()->profile())
        ->RegisterDownloadsDirectoryForTesting(my_files_dir_);
  }

  // Creates a file `test_file_name` in `parent_dir`.
  base::FilePath CreateTestFile(const std::string& test_file_name,
                                const base::FilePath& parent_dir) {
    const base::FilePath copied_file_path =
        parent_dir.AppendASCII(test_file_name);
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      CHECK(WriteFile(copied_file_path, "42"));
      CHECK(base::PathExists(copied_file_path));
    }

    return copied_file_path;
  }

  void SetMockCleanupHandler() {
    if (!cleanup_handler_) {
      cleanup_handler_ =
          std::make_unique<testing::NiceMock<MockCleanupHandler>>();
    }
    manager()->SetCleanupHandlerForTesting(cleanup_handler_->GetWeakPtr());
  }

  LocalFilesMigrationManager* manager() {
    return LocalFilesMigrationManagerFactory::GetInstance()
        ->GetForBrowserContext(browser()->profile());
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
  base::FilePath my_files_dir_;
  ash::system::FakeStatisticsProvider statistics_provider_;
  std::unique_ptr<MockMigrationNotificationManager> notification_manager_ =
      nullptr;
  std::unique_ptr<testing::NiceMock<MockCleanupHandler>> cleanup_handler_ =
      nullptr;
  testing::StrictMock<MockMigrationObserver> observer_;
  testing::StrictMock<ash::MockUserDataAuthClient> userdataauth_;
};

class LocalFilesMigrationManagerLocationTest
    : public LocalFilesMigrationManagerTest,
      public ::testing::WithParamInterface<
          /*migration_destination*/ std::string> {
 public:
  static std::string ParamToName(const testing::TestParamInfo<ParamType> info) {
    return info.param;
  }

  LocalFilesMigrationManagerLocationTest() = default;
  ~LocalFilesMigrationManagerLocationTest() override = default;

 protected:
  std::string GetMigrationDestination() { return GetParam(); }
};

IN_PROC_BROWSER_TEST_P(LocalFilesMigrationManagerLocationTest,
                       MigrationNotifiesObservers_Timeout) {
  SetUpMyFiles();
  base::FilePath source_file_path = CreateTestFile(kTestFile, my_files_dir_);

  base::ScopedMockTimeMessageLoopTaskRunner task_runner;
  EXPECT_CALL(observer_, OnMigrationSucceeded).Times(1);

  EXPECT_CALL(*notification_manager_,
              ShowMigrationInfoDialog(
                  _, TimeNear(base::Time::Now() + kTotalMigrationTimeout), _))
      .Times(2);

  std::unique_ptr<MockMigrationCoordinator> coordinator =
      std::make_unique<MockMigrationCoordinator>(browser()->profile());

  EXPECT_CALL(*coordinator.get(),
              Run(GetCloudProvider(GetMigrationDestination()),
                  std::vector<base::FilePath>({source_file_path}),
                  ExpectedUploadRootName(), _))
      .WillOnce([](MigrationDestination destination,
                   std::vector<base::FilePath> file_paths,
                   const std::string& upload_root,
                   MigrationDoneCallback callback) {
        std::move(callback).Run({}, base::FilePath(), base::FilePath());
      });

  manager()->SetCoordinatorForTesting(std::move(coordinator));

  SetMockCleanupHandler();

  base::RunLoop run_loop;
  // Write access will be disallowed.
  EXPECT_CALL(userdataauth_,
              SetUserDataStorageWriteEnabled(WithEnabled(false), _))
      .WillOnce(testing::DoAll(
          base::test::RunClosure(run_loop.QuitClosure()),
          ReplyWith(::user_data_auth::SetUserDataStorageWriteEnabledReply())));

  // Logged during initialization.
  histogram_tester_.ExpectBucketCount(
      "Enterprise.SkyVault.LocalStorage.Enabled", true, 1);

  // Changing the LocalUserFilesAllowed policy should trigger the migration and
  // update, after the timeout.
  SetMigrationPolicies(/*local_user_files_allowed=*/false,
                       /*destination=*/GetMigrationDestination());

  // Fast forward to the show the second dialog.
  task_runner->FastForwardBy(
      base::TimeDelta(kTotalMigrationTimeout - kFinalMigrationTimeout));
  // Fast forward again. The "now" doesn't advance so skip the full timeout.
  task_runner->FastForwardBy(base::TimeDelta(kTotalMigrationTimeout));
  run_loop.Run();

  histogram_tester_.ExpectBucketCount(
      "Enterprise.SkyVault.LocalStorage.Enabled", false, 1);
  histogram_tester_.ExpectBucketCount(
      GetUMAName(GetMigrationDestination(), kMigrationEnabledUMASuffix), true,
      1);
  histogram_tester_.ExpectBucketCount(
      GetUMAName(GetMigrationDestination(), kMigrationFailedUMASuffix), false,
      1);
  histogram_tester_.ExpectTotalCount(
      GetUMAName(GetMigrationDestination(), kMigrationSuccessDurationUMASuffix),
      1);
}

IN_PROC_BROWSER_TEST_P(LocalFilesMigrationManagerLocationTest,
                       MigrationNotifiesObservers_UploadNowFirstDialog) {
  SetUpMyFiles();
  base::FilePath source_file_path = CreateTestFile(kTestFile, my_files_dir_);

  base::ScopedMockTimeMessageLoopTaskRunner task_runner;
  EXPECT_CALL(observer_, OnMigrationSucceeded).Times(1);

  EXPECT_CALL(*notification_manager_,
              ShowMigrationInfoDialog(
                  _, TimeNear(base::Time::Now() + kTotalMigrationTimeout), _))
      .WillOnce([](MigrationDestination provider,
                   base::Time migration_start_time,
                   base::OnceClosure migration_callback) {
        std::move(migration_callback).Run();
      });

  std::unique_ptr<MockMigrationCoordinator> coordinator =
      std::make_unique<MockMigrationCoordinator>(browser()->profile());

  EXPECT_CALL(*coordinator.get(),
              Run(GetCloudProvider(GetMigrationDestination()),
                  std::vector<base::FilePath>({source_file_path}),
                  ExpectedUploadRootName(), _))
      .WillOnce([](MigrationDestination destination,
                   std::vector<base::FilePath> file_paths,
                   const std::string& upload_root,
                   MigrationDoneCallback callback) {
        std::move(callback).Run({}, base::FilePath(), base::FilePath());
      });

  manager()->SetCoordinatorForTesting(std::move(coordinator));

  SetMockCleanupHandler();

  base::RunLoop run_loop;
  // Write access will be disallowed.
  EXPECT_CALL(userdataauth_,
              SetUserDataStorageWriteEnabled(WithEnabled(false), _))
      .WillOnce(testing::DoAll(
          base::test::RunClosure(run_loop.QuitClosure()),
          ReplyWith(::user_data_auth::SetUserDataStorageWriteEnabledReply())));

  SetMigrationPolicies(/*local_user_files_allowed=*/false,
                       /*destination=*/GetMigrationDestination());
  task_runner->FastForwardBy(base::TimeDelta(base::Hours(5)));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_P(LocalFilesMigrationManagerLocationTest,
                       MigrationNotifiesObservers_UploadNowSecondDialog) {
  SetUpMyFiles();
  base::FilePath source_file_path = CreateTestFile(kTestFile, my_files_dir_);

  base::ScopedMockTimeMessageLoopTaskRunner task_runner;
  EXPECT_CALL(observer_, OnMigrationSucceeded).Times(1);

  EXPECT_CALL(*notification_manager_,
              ShowMigrationInfoDialog(
                  _, TimeNear(base::Time::Now() + kTotalMigrationTimeout), _))
      .WillOnce(testing::Return())
      .WillOnce([](MigrationDestination destination,
                   base::Time migration_start_time,
                   base::OnceClosure migration_callback) {
        std::move(migration_callback).Run();
      });

  std::unique_ptr<MockMigrationCoordinator> coordinator =
      std::make_unique<MockMigrationCoordinator>(browser()->profile());

  EXPECT_CALL(*coordinator.get(),
              Run(GetCloudProvider(GetMigrationDestination()),
                  std::vector<base::FilePath>({source_file_path}),
                  ExpectedUploadRootName(), _))
      .WillOnce([](MigrationDestination destination,
                   std::vector<base::FilePath> file_paths,
                   const std::string& upload_root,
                   MigrationDoneCallback callback) {
        std::move(callback).Run({}, base::FilePath(), base::FilePath());
      });

  manager()->SetCoordinatorForTesting(std::move(coordinator));

  SetMockCleanupHandler();

  base::RunLoop run_loop;
  // Write access will be disallowed.
  EXPECT_CALL(userdataauth_,
              SetUserDataStorageWriteEnabled(WithEnabled(false), _))
      .WillOnce(testing::DoAll(
          base::test::RunClosure(run_loop.QuitClosure()),
          ReplyWith(::user_data_auth::SetUserDataStorageWriteEnabledReply())));

  SetMigrationPolicies(/*local_user_files_allowed=*/false,
                       /*destination=*/GetMigrationDestination());
  // Fast forward only to the second dialog.
  task_runner->FastForwardBy(
      base::TimeDelta(kTotalMigrationTimeout - kFinalMigrationTimeout));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_P(LocalFilesMigrationManagerLocationTest,
                       CompletesIfEmpty) {
  EXPECT_CALL(observer_, OnMigrationSucceeded).Times(1);

  base::RunLoop run_loop;
  // Write access will be disallowed.
  EXPECT_CALL(userdataauth_,
              SetUserDataStorageWriteEnabled(WithEnabled(false), _))
      .WillOnce(testing::DoAll(
          base::test::RunClosure(run_loop.QuitClosure()),
          ReplyWith(::user_data_auth::SetUserDataStorageWriteEnabledReply())));

  SetMigrationPolicies(/*local_user_files_allowed=*/false,
                       /*destination=*/GetMigrationDestination());
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_P(LocalFilesMigrationManagerLocationTest,
                       NoMigrationIfLocalFilesAllowed) {
  EXPECT_CALL(observer_, OnMigrationReset).Times(1);
  // Write access will be explicitly allowed.
  EXPECT_CALL(userdataauth_,
              SetUserDataStorageWriteEnabled(WithEnabled(true), _))
      .WillOnce(
          ReplyWith(::user_data_auth::SetUserDataStorageWriteEnabledReply()));
  SetMigrationPolicies(/*local_user_files_allowed=*/true,
                       /*destination=*/GetMigrationDestination());
}

// Tests that if cloud provider for which migration is turned on is disallowed
// by other policies, a notification is shown and no migration happens.
IN_PROC_BROWSER_TEST_P(LocalFilesMigrationManagerLocationTest,
                       NoMigrationIfMisconfigured) {
  const std::string destination = GetMigrationDestination();
  MigrationDestination provider;
  // Disable the cloud storage before setting SkyVault policies.
  if (destination == download_dir_util::kLocationGoogleDrive) {
    drive::DriveIntegrationServiceFactory::FindForProfile(browser()->profile())
        ->SetEnabled(false);
    provider = MigrationDestination::kGoogleDrive;
  } else {
    SetOneDrivePolicy("disallowed");
    provider = MigrationDestination::kOneDrive;
  }

  EXPECT_CALL(*notification_manager_.get(),
              ShowConfigurationErrorNotification(provider))
      .Times(1);

  SetMigrationPolicies(/*local_user_files_allowed=*/false, destination);

  histogram_tester_.ExpectBucketCount(
      "Enterprise.SkyVault.LocalStorage.Enabled", false, 1);
  // DownloadDirectory isn't set, so local storage is also miconfigured.
  histogram_tester_.ExpectBucketCount(
      "Enterprise.SkyVault.LocalStorage.Misconfigured", true, 1);
  histogram_tester_.ExpectBucketCount(
      GetUMAName(destination, kMigrationEnabledUMASuffix), true, 1);
  histogram_tester_.ExpectBucketCount(
      GetUMAName(destination, kMigrationMisconfiguredUMASuffix), true, 1);
}

IN_PROC_BROWSER_TEST_F(LocalFilesMigrationManagerTest,
                       MigrationCompleteIfNoDestinationAndEmpty) {
  EXPECT_CALL(observer_, OnMigrationReset).Times(1);
  EXPECT_CALL(observer_, OnMigrationSucceeded).Times(1);
  base::RunLoop run_loop;
  // Write access will be disallowed.
  EXPECT_CALL(userdataauth_,
              SetUserDataStorageWriteEnabled(WithEnabled(false), _))
      .WillOnce(testing::DoAll(
          base::test::RunClosure(run_loop.QuitClosure()),
          ReplyWith(::user_data_auth::SetUserDataStorageWriteEnabledReply())));
  SetMigrationPolicies(/*local_user_files_allowed=*/false,
                       /*destination=*/kReadOnly);
  run_loop.Run();

  histogram_tester_.ExpectBucketCount(
      "Enterprise.SkyVault.LocalStorage.Enabled", false, 1);
}

IN_PROC_BROWSER_TEST_F(LocalFilesMigrationManagerTest,
                       NoMigrationIfNoDestinationAndNonEmpty) {
  SetUpMyFiles();
  CreateTestFile(kTestFile, my_files_dir_);

  EXPECT_CALL(observer_, OnMigrationReset).Times(1);
  SetMigrationPolicies(/*local_user_files_allowed=*/false,
                       /*destination=*/kReadOnly);

  histogram_tester_.ExpectBucketCount(
      "Enterprise.SkyVault.LocalStorage.Enabled", false, 1);
}

IN_PROC_BROWSER_TEST_F(LocalFilesMigrationManagerTest,
                       EnableLocalFilesStopsMigration) {
  EXPECT_CALL(observer_, OnMigrationReset).Times(1);
  SetUpMyFiles();
  base::FilePath source_file_path = CreateTestFile(kTestFile, my_files_dir_);

  base::ScopedMockTimeMessageLoopTaskRunner task_runner;

  std::unique_ptr<MockMigrationCoordinator> coordinator =
      std::make_unique<MockMigrationCoordinator>(browser()->profile());
  {
    testing::InSequence s;
    EXPECT_CALL(*coordinator.get(),
                Run(MigrationDestination::kGoogleDrive,
                    std::vector<base::FilePath>({source_file_path}),
                    ExpectedUploadRootName(), _))
        .Times(1);
    EXPECT_CALL(*coordinator.get(), Cancel).Times(1);
  }

  manager()->SetCoordinatorForTesting(std::move(coordinator));

  // Enable migration to Google Drive.
  SetMigrationPolicies(/*local_user_files_allowed=*/false,
                       /*destination=*/download_dir_util::kLocationGoogleDrive);
  task_runner->FastForwardBy(
      base::TimeDelta(kTotalMigrationTimeout - kFinalMigrationTimeout));
  task_runner->FastForwardBy(base::TimeDelta(kTotalMigrationTimeout));
  // Allow local storage: stops the migration and enables write to ensure.
  EXPECT_CALL(userdataauth_,
              SetUserDataStorageWriteEnabled(WithEnabled(true), _))
      .WillOnce(
          ReplyWith(::user_data_auth::SetUserDataStorageWriteEnabledReply()));
  SetMigrationPolicies(/*local_user_files_allowed=*/true,
                       /*destination=*/download_dir_util::kLocationOneDrive);

  task_runner->FastForwardBy(
      base::TimeDelta(kTotalMigrationTimeout - kFinalMigrationTimeout));
  task_runner->FastForwardBy(base::TimeDelta(kTotalMigrationTimeout));
}

IN_PROC_BROWSER_TEST_F(LocalFilesMigrationManagerTest,
                       ChangeDestinationStopsMigration) {
  EXPECT_CALL(observer_, OnMigrationReset).Times(1);
  SetUpMyFiles();
  base::FilePath source_file_path = CreateTestFile(kTestFile, my_files_dir_);

  base::ScopedMockTimeMessageLoopTaskRunner task_runner;
  EXPECT_CALL(observer_, OnMigrationSucceeded).Times(1);

  std::unique_ptr<MockMigrationCoordinator> coordinator =
      std::make_unique<MockMigrationCoordinator>(browser()->profile());
  {
    testing::InSequence s;
    EXPECT_CALL(*coordinator.get(), Run(MigrationDestination::kOneDrive, _,
                                        ExpectedUploadRootName(), _))
        .Times(1);
    EXPECT_CALL(*coordinator.get(), Cancel).Times(1);
    EXPECT_CALL(*coordinator.get(), Run(MigrationDestination::kGoogleDrive, _,
                                        ExpectedUploadRootName(), _))
        .WillOnce([](MigrationDestination destination,
                     std::vector<base::FilePath> file_paths,
                     const std::string& destination_dir,
                     MigrationDoneCallback callback) {
          // Finish without delay.
          std::move(callback).Run(/*errors=*/{}, base::FilePath(),
                                  base::FilePath());
        });
  }

  manager()->SetCoordinatorForTesting(std::move(coordinator));

  SetMockCleanupHandler();

  // Enable migration to OneDrive.
  SetMigrationPolicies(/*local_user_files_allowed=*/false,
                       /*destination=*/download_dir_util::kLocationOneDrive);
  task_runner->FastForwardBy(
      base::TimeDelta(kTotalMigrationTimeout - kFinalMigrationTimeout));
  task_runner->FastForwardBy(base::TimeDelta(kTotalMigrationTimeout));

  base::RunLoop run_loop;
  // Write access will be disallowed.
  EXPECT_CALL(userdataauth_,
              SetUserDataStorageWriteEnabled(WithEnabled(false), _))
      .WillOnce(testing::DoAll(
          base::test::RunClosure(run_loop.QuitClosure()),
          ReplyWith(::user_data_auth::SetUserDataStorageWriteEnabledReply())));
  // Enable migration to Google Drive: first upload stops, a new one starts.
  SetMigrationPolicies(/*local_user_files_allowed=*/false,
                       /*destination=*/download_dir_util::kLocationGoogleDrive);
  task_runner->FastForwardBy(
      base::TimeDelta(kTotalMigrationTimeout - kFinalMigrationTimeout));
  task_runner->FastForwardBy(base::TimeDelta(kTotalMigrationTimeout));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(LocalFilesMigrationManagerTest,
                       NoDestinationStopsMigration) {
  EXPECT_CALL(observer_, OnMigrationReset).Times(1);
  SetUpMyFiles();
  base::FilePath source_file_path = CreateTestFile(kTestFile, my_files_dir_);

  base::ScopedMockTimeMessageLoopTaskRunner task_runner;

  std::unique_ptr<MockMigrationCoordinator> coordinator =
      std::make_unique<MockMigrationCoordinator>(browser()->profile());
  {
    testing::InSequence s;
    EXPECT_CALL(*coordinator.get(), Run(MigrationDestination::kOneDrive, _,
                                        ExpectedUploadRootName(), _))
        .Times(1);
    EXPECT_CALL(*coordinator.get(), Cancel).Times(1);
  }

  manager()->SetCoordinatorForTesting(std::move(coordinator));

  // Enable migration to OneDrive.
  SetMigrationPolicies(/*local_user_files_allowed=*/false,
                       /*destination=*/download_dir_util::kLocationOneDrive);
  task_runner->FastForwardBy(
      base::TimeDelta(kTotalMigrationTimeout - kFinalMigrationTimeout));
  task_runner->FastForwardBy(base::TimeDelta(kTotalMigrationTimeout));
  // Set migration to "read_only": stops the migration.
  SetMigrationPolicies(/*local_user_files_allowed=*/false,
                       /*destination=*/kReadOnly);

  task_runner->FastForwardBy(
      base::TimeDelta(kTotalMigrationTimeout - kFinalMigrationTimeout));
  task_runner->FastForwardBy(base::TimeDelta(kTotalMigrationTimeout));
}

// Tests that when LocalFilesMigrationDestination policy is set to "delete", the
// flow completes without errors.
IN_PROC_BROWSER_TEST_F(LocalFilesMigrationManagerTest, DeleteLocalFiles) {
  EXPECT_CALL(observer_, OnMigrationSucceeded).Times(1);
  SetUpMyFiles();
  base::FilePath source_file_path = CreateTestFile(kTestFile, my_files_dir_);

  base::ScopedMockTimeMessageLoopTaskRunner task_runner;

  SetMockCleanupHandler();
  // Since it's the delete case, ensure that the cleanup is called.
  EXPECT_CALL(*cleanup_handler_, Cleanup)
      .WillOnce(
          [](base::OnceCallback<void(
                 const std::optional<std::string>& error_message)> callback) {
            std::move(callback).Run(std::nullopt);
          });

  // Write access will be disallowed.
  EXPECT_CALL(userdataauth_, SetUserDataStorageWriteEnabled)
      .WillOnce(
          ReplyWith(::user_data_auth::SetUserDataStorageWriteEnabledReply()));

  // Set the policy to delete local files.
  SetMigrationPolicies(/*local_user_files_allowed=*/false,
                       /*destination=*/"delete");
  task_runner->FastForwardBy(
      base::TimeDelta(kTotalMigrationTimeout - kFinalMigrationTimeout));
  task_runner->FastForwardBy(base::TimeDelta(kTotalMigrationTimeout));
}

// Tests that when LocalFilesMigrationDestination policy is set to "delete", a
// failed deletion is retried.
IN_PROC_BROWSER_TEST_F(LocalFilesMigrationManagerTest,
                       DeleteLocalFilesRetriesOnFailure) {
  EXPECT_CALL(observer_, OnMigrationSucceeded).Times(1);
  SetUpMyFiles();
  base::FilePath source_file_path = CreateTestFile(kTestFile, my_files_dir_);

  base::ScopedMockTimeMessageLoopTaskRunner task_runner;

  SetMockCleanupHandler();

  base::test::TestFuture<void> cleanup_called_again;
  EXPECT_CALL(*cleanup_handler_, Cleanup)
      .WillOnce(
          [](base::OnceCallback<void(
                 const std::optional<std::string>& error_message)> callback) {
            std::move(callback).Run("Something failed");
          })
      .WillOnce(
          [&cleanup_called_again](
              base::OnceCallback<void(
                  const std::optional<std::string>& error_message)> callback) {
            std::move(callback).Run(std::nullopt);
            cleanup_called_again.SetValue();
          });

  // Write access will be disallowed.
  EXPECT_CALL(userdataauth_, SetUserDataStorageWriteEnabled)
      .WillOnce(
          ReplyWith(::user_data_auth::SetUserDataStorageWriteEnabledReply()));

  SetMigrationPolicies(/*local_user_files_allowed=*/false,
                       /*destination=*/"delete");
  task_runner->FastForwardBy(
      base::TimeDelta(kTotalMigrationTimeout - kFinalMigrationTimeout));
  task_runner->FastForwardBy(base::TimeDelta(kTotalMigrationTimeout));

  ASSERT_TRUE(cleanup_called_again.Wait())
      << "Cleanup wasn't called the second time";

  histogram_tester_.ExpectBucketCount(
      "Enterprise.SkyVault.Migration.Delete.CleanupError", true, 1);
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.SkyVault.Migration.Delete.Retry",
      /*sample=*/1.0,
      /*expected_bucket_count=*/1);
  histogram_tester_.ExpectBucketCount(
      "Enterprise.SkyVault.Migration.Delete.Failed", false, 1);
}

INSTANTIATE_TEST_SUITE_P(
    LocalUserFiles,
    LocalFilesMigrationManagerLocationTest,
    ::testing::Values(download_dir_util::kLocationGoogleDrive,
                      download_dir_util::kLocationOneDrive),
    LocalFilesMigrationManagerLocationTest::ParamToName);

}  // namespace policy::local_user_files
