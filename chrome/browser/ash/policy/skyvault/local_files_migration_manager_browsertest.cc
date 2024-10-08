// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/local_files_migration_manager.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/skyvault/local_files_migration_constants.h"
#include "chrome/browser/ash/policy/skyvault/migration_coordinator.h"
#include "chrome/browser/ash/policy/skyvault/migration_notification_manager.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/ash/policy/skyvault/test/skyvault_test_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_dir_util.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/ash/components/dbus/userdataauth/mock_userdataauth_client.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test.h"
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

// Matcher for scheduled migration time.
MATCHER_P(TimeNear, expected_time, "") {
  base::TimeDelta delta = (arg - expected_time).magnitude();
  return delta <= kMaxDelta;
}

// Constructs the expected destination directory name.
std::string ExpectedDestinationDirName() {
  return std::string(kDestinationDirName) + " " +
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

}  // namespace

// TODO(b/352539894): Add tests with some files to upload.
class LocalFilesMigrationManagerTest : public policy::PolicyTest {
 public:
  LocalFilesMigrationManagerTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kSkyVault, features::kSkyVaultV2,
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

  LocalFilesMigrationManager* manager() {
    return LocalFilesMigrationManagerFactory::GetInstance()
        ->GetForBrowserContext(browser()->profile());
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
  ash::system::FakeStatisticsProvider statistics_provider_;
  std::unique_ptr<MockMigrationNotificationManager> notification_manager_ =
      nullptr;
  MockMigrationObserver observer_;
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
  std::string MigrationDestination() { return GetParam(); }
};

IN_PROC_BROWSER_TEST_P(LocalFilesMigrationManagerLocationTest,
                       MigrationNotifiesObservers_Timeout) {
  base::ScopedMockTimeMessageLoopTaskRunner task_runner;
  EXPECT_CALL(observer_, OnMigrationSucceeded).Times(1);

  EXPECT_CALL(*notification_manager_,
              ShowMigrationInfoDialog(
                  _, TimeNear(base::Time::Now() + kTotalMigrationTimeout), _))
      .Times(2);

  // Logged during initialization.
  histogram_tester_.ExpectBucketCount(
      "Enterprise.SkyVault.LocalStorage.Enabled", true, 1);

  // Changing the LocalUserFilesAllowed policy should trigger the migration and
  // update, after the timeout.
  SetMigrationPolicies(/*local_user_files_allowed=*/false,
                       /*destination=*/MigrationDestination());

  // Fast forward to the show the second dialog.
  task_runner->FastForwardBy(
      base::TimeDelta(kTotalMigrationTimeout - kFinalMigrationTimeout));
  // Fast forward again. The "now" doesn't advance so skip the full timeout.
  task_runner->FastForwardBy(base::TimeDelta(kTotalMigrationTimeout));

  histogram_tester_.ExpectBucketCount(
      "Enterprise.SkyVault.LocalStorage.Enabled", false, 1);
  const std::string provider =
      (MigrationDestination() == download_dir_util::kLocationGoogleDrive)
          ? "GoogleDrive"
          : "OneDrive";
  histogram_tester_.ExpectBucketCount(
      GetUMAName(MigrationDestination(), kMigrationEnabledUMASuffix), true, 1);
}

IN_PROC_BROWSER_TEST_P(LocalFilesMigrationManagerLocationTest,
                       MigrationNotifiesObservers_UploadNowFirstDialog) {
  base::ScopedMockTimeMessageLoopTaskRunner task_runner;
  EXPECT_CALL(observer_, OnMigrationSucceeded).Times(1);

  EXPECT_CALL(*notification_manager_,
              ShowMigrationInfoDialog(
                  _, TimeNear(base::Time::Now() + kTotalMigrationTimeout), _))
      .WillOnce([](CloudProvider provider, base::Time migration_start_time,
                   base::OnceClosure migration_callback) {
        std::move(migration_callback).Run();
      });

  // Write access will be disallowed.
  EXPECT_CALL(userdataauth_,
              SetUserDataStorageWriteEnabled(WithEnabled(false), _))
      .Times(1)
      .WillRepeatedly(
          ReplyWith(::user_data_auth::SetUserDataStorageWriteEnabledReply()));
  SetMigrationPolicies(/*local_user_files_allowed=*/false,
                       /*destination=*/MigrationDestination());
  task_runner->FastForwardBy(base::TimeDelta(base::Hours(5)));
}

IN_PROC_BROWSER_TEST_P(LocalFilesMigrationManagerLocationTest,
                       MigrationNotifiesObservers_UploadNowSecondDialog) {
  base::ScopedMockTimeMessageLoopTaskRunner task_runner;
  EXPECT_CALL(observer_, OnMigrationSucceeded).Times(1);

  EXPECT_CALL(*notification_manager_,
              ShowMigrationInfoDialog(
                  _, TimeNear(base::Time::Now() + kTotalMigrationTimeout), _))
      .WillOnce(testing::Return())
      .WillOnce([](CloudProvider provider, base::Time migration_start_time,
                   base::OnceClosure migration_callback) {
        std::move(migration_callback).Run();
      });

  SetMigrationPolicies(/*local_user_files_allowed=*/false,
                       /*destination=*/MigrationDestination());
  // Fast forward only to the second dialog.
  task_runner->FastForwardBy(
      base::TimeDelta(kTotalMigrationTimeout - kFinalMigrationTimeout));
}

IN_PROC_BROWSER_TEST_P(LocalFilesMigrationManagerLocationTest,
                       NoMigrationIfLocalFilesAllowed) {
  EXPECT_CALL(observer_, OnMigrationSucceeded).Times(0);

  // Write access will be explicitly allowed.
  EXPECT_CALL(userdataauth_,
              SetUserDataStorageWriteEnabled(WithEnabled(true), _))
      .Times(1)
      .WillRepeatedly(
          ReplyWith(::user_data_auth::SetUserDataStorageWriteEnabledReply()));
  SetMigrationPolicies(/*local_user_files_allowed=*/true,
                       /*destination=*/MigrationDestination());
}

IN_PROC_BROWSER_TEST_P(LocalFilesMigrationManagerLocationTest,
                       NoMigrationIfDisabled) {
  EXPECT_CALL(observer_, OnMigrationSucceeded).Times(0);
  LocalFilesMigrationManager manager(browser()->profile());

  SetMigrationPolicies(/*local_user_files_allowed=*/false,
                       /*destination=*/MigrationDestination());
}

// Tests that if cloud provider for which migration is turned on is disallowed
// by other policies, a notification is shown and no migration happens.
IN_PROC_BROWSER_TEST_P(LocalFilesMigrationManagerLocationTest,
                       NoMigrationIfMisconfigured) {
  EXPECT_CALL(observer_, OnMigrationSucceeded).Times(0);

  const std::string destination = MigrationDestination();
  CloudProvider provider;
  // Disable the cloud storage before setting SkyVault policies.
  if (destination == download_dir_util::kLocationGoogleDrive) {
    drive::DriveIntegrationServiceFactory::FindForProfile(browser()->profile())
        ->SetEnabled(false);
    provider = CloudProvider::kGoogleDrive;
  } else {
    SetOneDrivePolicy("disallowed");
    provider = CloudProvider::kOneDrive;
  }

  EXPECT_CALL(*notification_manager_.get(),
              ShowConfigurationErrorNotification(provider))
      .Times(1);

  SetMigrationPolicies(/*local_user_files_allowed=*/false, destination);

  histogram_tester_.ExpectBucketCount(
      "Enterprise.SkyVault.LocalStorage.Enabled", false, 1);
  histogram_tester_.ExpectBucketCount(
      GetUMAName(destination, kMigrationEnabledUMASuffix), true, 1);
  histogram_tester_.ExpectBucketCount(
      GetUMAName(destination, kMigrationMisconfiguredUMASuffix), true, 1);
}

IN_PROC_BROWSER_TEST_F(LocalFilesMigrationManagerTest,
                       NoMigrationIfNoDefaultLocation) {
  EXPECT_CALL(observer_, OnMigrationSucceeded).Times(0);
  LocalFilesMigrationManager manager(browser()->profile());

  SetMigrationPolicies(/*local_user_files_allowed=*/false,
                       /*destination=*/kReadOnly);
}

IN_PROC_BROWSER_TEST_F(LocalFilesMigrationManagerTest,
                       EnableLocalFilesStopsMigration) {
  base::ScopedMockTimeMessageLoopTaskRunner task_runner;
  EXPECT_CALL(observer_, OnMigrationSucceeded).Times(0);

  std::unique_ptr<MockMigrationCoordinator> coordinator =
      std::make_unique<MockMigrationCoordinator>(browser()->profile());
  {
    testing::InSequence s;
    EXPECT_CALL(*coordinator.get(), Run(CloudProvider::kGoogleDrive, _,
                                        ExpectedDestinationDirName(), _))
        .Times(1);
    EXPECT_CALL(*coordinator.get(), Stop).Times(1);
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
      .Times(1)
      .WillRepeatedly(
          ReplyWith(::user_data_auth::SetUserDataStorageWriteEnabledReply()));
  SetMigrationPolicies(/*local_user_files_allowed=*/true,
                       /*destination=*/download_dir_util::kLocationOneDrive);

  task_runner->FastForwardBy(
      base::TimeDelta(kTotalMigrationTimeout - kFinalMigrationTimeout));
  task_runner->FastForwardBy(base::TimeDelta(kTotalMigrationTimeout));
}

IN_PROC_BROWSER_TEST_F(LocalFilesMigrationManagerTest,
                       ChangeDestinationStopsMigration) {
  base::ScopedMockTimeMessageLoopTaskRunner task_runner;
  EXPECT_CALL(observer_, OnMigrationSucceeded).Times(1);

  std::unique_ptr<MockMigrationCoordinator> coordinator =
      std::make_unique<MockMigrationCoordinator>(browser()->profile());
  {
    testing::InSequence s;
    EXPECT_CALL(*coordinator.get(), Run(CloudProvider::kOneDrive, _,
                                        ExpectedDestinationDirName(), _))
        .Times(1);
    EXPECT_CALL(*coordinator.get(), Stop).Times(1);
    EXPECT_CALL(*coordinator.get(), Run(CloudProvider::kGoogleDrive, _,
                                        ExpectedDestinationDirName(), _))
        .WillOnce([](CloudProvider cloud_provider,
                     std::vector<base::FilePath> file_paths,
                     const std::string& destination_dir,
                     MigrationDoneCallback callback) {
          // Finish without delay.
          std::move(callback).Run(/*errors=*/{});
        });
  }

  manager()->SetCoordinatorForTesting(std::move(coordinator));

  // Enable migration to OneDrive.
  SetMigrationPolicies(/*local_user_files_allowed=*/false,
                       /*destination=*/download_dir_util::kLocationOneDrive);
  task_runner->FastForwardBy(
      base::TimeDelta(kTotalMigrationTimeout - kFinalMigrationTimeout));
  task_runner->FastForwardBy(base::TimeDelta(kTotalMigrationTimeout));

  // Write access will be disallowed.
  EXPECT_CALL(userdataauth_,
              SetUserDataStorageWriteEnabled(WithEnabled(false), _))
      .Times(1)
      .WillRepeatedly(
          ReplyWith(::user_data_auth::SetUserDataStorageWriteEnabledReply()));
  // Enable migration to Google Drive: first upload stops, a new one starts.
  SetMigrationPolicies(/*local_user_files_allowed=*/false,
                       /*destination=*/download_dir_util::kLocationGoogleDrive);
  task_runner->FastForwardBy(
      base::TimeDelta(kTotalMigrationTimeout - kFinalMigrationTimeout));
  task_runner->FastForwardBy(base::TimeDelta(kTotalMigrationTimeout));
}

IN_PROC_BROWSER_TEST_F(LocalFilesMigrationManagerTest,
                       NoDestinationStopsMigration) {
  base::ScopedMockTimeMessageLoopTaskRunner task_runner;
  EXPECT_CALL(observer_, OnMigrationSucceeded).Times(0);

  std::unique_ptr<MockMigrationCoordinator> coordinator =
      std::make_unique<MockMigrationCoordinator>(browser()->profile());
  {
    testing::InSequence s;
    EXPECT_CALL(*coordinator.get(), Run(CloudProvider::kOneDrive, _,
                                        ExpectedDestinationDirName(), _))
        .Times(1);
    EXPECT_CALL(*coordinator.get(), Stop).Times(1);
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

INSTANTIATE_TEST_SUITE_P(
    LocalUserFiles,
    LocalFilesMigrationManagerLocationTest,
    ::testing::Values(download_dir_util::kLocationGoogleDrive,
                      download_dir_util::kLocationOneDrive),
    LocalFilesMigrationManagerLocationTest::ParamToName);

}  // namespace policy::local_user_files
