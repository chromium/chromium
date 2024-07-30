// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/local_files_migration_manager.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/skyvault/migration_coordinator.h"
#include "chrome/browser/ash/policy/skyvault/migration_notification_manager.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_dir_util.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/ash/skyvault/local_files_migration_dialog.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/ash/components/dbus/userdataauth/mock_userdataauth_client.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace policy::local_user_files {

namespace {
constexpr char kReadOnly[] = "read_only";

constexpr char kEmail[] = "stub-user@example.com";

// Matcher for `SetUserDataStorageWriteEnabledRequest`.
MATCHER_P(WithEnabled, enabled, "") {
  return arg.account_id().account_id() == kEmail && arg.enabled() == enabled;
}

// GMock action that runs the callback (which is expected to be the second
// argument in the mocked function) with the given reply.
template <typename ReplyType>
auto ReplyWith(const ReplyType& reply) {
  return base::test::RunOnceCallbackRepeatedly<1>(reply);
}

class MockMigrationObserver : public LocalFilesMigrationManager::Observer {
 public:
  MockMigrationObserver() = default;
  ~MockMigrationObserver() = default;

  MOCK_METHOD(void, OnMigrationSucceeded, (), (override));
};

// Mock implementation of MigrationNotificationManager.
class MockMigrationNotificationManager : public MigrationNotificationManager {
 public:
  explicit MockMigrationNotificationManager(content::BrowserContext* context)
      : MigrationNotificationManager(context) {}

  MOCK_METHOD(void,
              ShowMigrationInfoDialog,
              (CloudProvider, base::TimeDelta, base::OnceClosure),
              (override));
};

// Mock implementation of MigrationUploadHandler.
class MockMigrationCoordinator : public MigrationCoordinator {
 public:
  explicit MockMigrationCoordinator(Profile* profile)
      : MigrationCoordinator(profile) {
    ON_CALL(*this, Run)
        .WillByDefault([this](CloudProvider cloud_provider,
                              std::vector<base::FilePath> file_paths,
                              const std::string& destination_dir,
                              MigrationDoneCallback callback) {
          is_running_ = true;
          // Simulate upload lasting a while.
          base::SequencedTaskRunner::GetCurrentDefault()
              ->GetCurrentDefault()
              ->PostDelayedTask(
                  FROM_HERE,
                  base::BindOnce(
                      &MockMigrationCoordinator::OnMigrationDone,
                      weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                      std::map<base::FilePath, MigrationUploadError>()),
                  base::Minutes(5));  // Delay 5 minutes
        });

    ON_CALL(*this, Stop).WillByDefault([this]() { is_running_ = false; });
  }
  ~MockMigrationCoordinator() override = default;

  bool IsRunning() const override { return is_running_; }

  void OnMigrationDone(
      MigrationDoneCallback callback,
      std::map<base::FilePath, MigrationUploadError> errors) override {
    if (is_running_) {
      std::move(callback).Run(std::move(errors));
      is_running_ = false;
    }
  }

  MOCK_METHOD(void,
              Run,
              (CloudProvider cloud_provider,
               std::vector<base::FilePath> file_paths,
               const std::string& destination_dir,
               MigrationDoneCallback callback),
              (override));
  MOCK_METHOD(void, Stop, (), (override));

 private:
  bool is_running_ = false;

  base::WeakPtrFactory<MockMigrationCoordinator> weak_ptr_factory_{this};
};

}  // namespace

// TODO(b/352539894): Add tests with some files to upload.
class LocalFilesMigrationManagerTest : public policy::PolicyTest {
 public:
  LocalFilesMigrationManagerTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kSkyVault, features::kSkyVaultV2},
        /*disabled_features=*/{});
  }
  ~LocalFilesMigrationManagerTest() override = default;

  void SetUpOnMainThread() override {
    manager_ =
        LocalFilesMigrationManagerFactory::GetInstance()->GetForBrowserContext(
            browser()->profile());
    ASSERT_TRUE(manager_);

    manager_->AddObserver(&observer_);

    notification_manager_ = std::make_unique<MockMigrationNotificationManager>(
        browser()->profile());
    manager_->SetNotificationManagerForTesting(notification_manager_.get());

    ash::UserDataAuthClient::OverrideGlobalInstanceForTesting(&userdataauth_);
  }

  void TearDownOnMainThread() override {
    ash::UserDataAuthClient::OverrideGlobalInstanceForTesting(
        ash::FakeUserDataAuthClient::Get());

    manager_->SetNotificationManagerForTesting(
        MigrationNotificationManagerFactory::GetInstance()
            ->GetForBrowserContext(browser()->profile()));

    manager_ = nullptr;
    notification_manager_.reset();
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

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<MockMigrationNotificationManager> notification_manager_ =
      nullptr;
  raw_ptr<LocalFilesMigrationManager> manager_ = nullptr;
  MockMigrationObserver observer_;
  testing::StrictMock<ash::MockUserDataAuthClient> userdataauth_;
};

class LocalFilesMigrationManagerLocationTest
    : public LocalFilesMigrationManagerTest,
      public ::testing::WithParamInterface</*default_location*/ std::string> {
 public:
  static std::string ParamToName(const testing::TestParamInfo<ParamType> info) {
    return info.param;
  }

  LocalFilesMigrationManagerLocationTest() = default;
  ~LocalFilesMigrationManagerLocationTest() = default;

 protected:
  std::string MigrationDestination() { return GetParam(); }
};

IN_PROC_BROWSER_TEST_P(LocalFilesMigrationManagerLocationTest,
                       MigrationNotifiesObservers_Timeout) {
  base::ScopedMockTimeMessageLoopTaskRunner task_runner;
  EXPECT_CALL(observer_, OnMigrationSucceeded).Times(1);

  EXPECT_CALL(*notification_manager_,
              ShowMigrationInfoDialog(testing::_, base::Hours(24), testing::_));
  EXPECT_CALL(*notification_manager_,
              ShowMigrationInfoDialog(testing::_, base::Hours(1), testing::_));

  ASSERT_TRUE(manager_);

  // Changing the LocalUserFilesAllowed policy should trigger the migration and
  // update, after the timeout.
  SetMigrationPolicies(/*local_user_files_allowed=*/false,
                       /*destination=*/MigrationDestination());
  // Fast forward to start automatically.
  task_runner->FastForwardBy(base::TimeDelta(base::Hours(24)));
}

IN_PROC_BROWSER_TEST_P(LocalFilesMigrationManagerLocationTest,
                       MigrationNotifiesObservers_UploadNowFirstDialog) {
  base::ScopedMockTimeMessageLoopTaskRunner task_runner;
  EXPECT_CALL(observer_, OnMigrationSucceeded).Times(1);

  EXPECT_CALL(*notification_manager_,
              ShowMigrationInfoDialog(testing::_, base::Hours(24), testing::_))
      .WillOnce([](CloudProvider provider, base::TimeDelta migration_delay,
                   base::OnceClosure migration_callback) {
        std::move(migration_callback).Run();
      });

  ASSERT_TRUE(manager_);

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
              ShowMigrationInfoDialog(testing::_, base::Hours(24), testing::_));
  EXPECT_CALL(*notification_manager_,
              ShowMigrationInfoDialog(testing::_, base::Hours(1), testing::_))
      .WillOnce([](CloudProvider provider, base::TimeDelta migration_delay,
                   base::OnceClosure migration_callback) {
        std::move(migration_callback).Run();
      });

  SetMigrationPolicies(/*local_user_files_allowed=*/false,
                       /*destination=*/MigrationDestination());
  // Fast forward only to the second dialog.
  task_runner->FastForwardBy(base::TimeDelta(base::Hours(23)));
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
    EXPECT_CALL(*coordinator.get(), Run(CloudProvider::kGoogleDrive, testing::_,
                                        testing::_, testing::_))
        .Times(1);
    EXPECT_CALL(*coordinator.get(), Stop).Times(1);
  }

  manager_->SetCoordinatorForTesting(std::move(coordinator));

  // Enable migration to Google Drive.
  SetMigrationPolicies(/*local_user_files_allowed=*/false,
                       /*destination=*/download_dir_util::kLocationGoogleDrive);
  task_runner->FastForwardBy(base::TimeDelta(base::Hours(24)));
  // Allow local storage: stops the migration and enables write to ensure.
  EXPECT_CALL(userdataauth_,
              SetUserDataStorageWriteEnabled(WithEnabled(true), _))
      .Times(1)
      .WillRepeatedly(
          ReplyWith(::user_data_auth::SetUserDataStorageWriteEnabledReply()));
  SetMigrationPolicies(/*local_user_files_allowed=*/true,
                       /*destination=*/download_dir_util::kLocationOneDrive);
  task_runner->FastForwardBy(base::TimeDelta(base::Hours(24)));
}

IN_PROC_BROWSER_TEST_F(LocalFilesMigrationManagerTest,
                       ChangeDestinationStopsMigration) {
  base::ScopedMockTimeMessageLoopTaskRunner task_runner;
  EXPECT_CALL(observer_, OnMigrationSucceeded).Times(1);

  std::unique_ptr<MockMigrationCoordinator> coordinator =
      std::make_unique<MockMigrationCoordinator>(browser()->profile());
  {
    testing::InSequence s;
    EXPECT_CALL(*coordinator.get(), Run(CloudProvider::kOneDrive, testing::_,
                                        testing::_, testing::_))
        .Times(1);
    EXPECT_CALL(*coordinator.get(), Stop).Times(1);
    EXPECT_CALL(*coordinator.get(), Run(CloudProvider::kGoogleDrive, testing::_,
                                        testing::_, testing::_))
        .WillOnce([](CloudProvider cloud_provider,
                     std::vector<base::FilePath> file_paths,
                     const std::string& destination_dir,
                     MigrationDoneCallback callback) {
          // Finish without delay.
          std::move(callback).Run(/*errors=*/{});
        });
  }

  manager_->SetCoordinatorForTesting(std::move(coordinator));

  // Enable migration to OneDrive.
  SetMigrationPolicies(/*local_user_files_allowed=*/false,
                       /*destination=*/download_dir_util::kLocationOneDrive);
  task_runner->FastForwardBy(base::TimeDelta(base::Hours(24)));
  // Enable migration to Google Drive: first upload stops, a new one starts.
  SetMigrationPolicies(/*local_user_files_allowed=*/false,
                       /*destination=*/download_dir_util::kLocationGoogleDrive);
  task_runner->FastForwardBy(base::TimeDelta(base::Hours(24)));
}

IN_PROC_BROWSER_TEST_F(LocalFilesMigrationManagerTest,
                       NoDestinationStopsMigration) {
  base::ScopedMockTimeMessageLoopTaskRunner task_runner;
  EXPECT_CALL(observer_, OnMigrationSucceeded).Times(0);

  std::unique_ptr<MockMigrationCoordinator> coordinator =
      std::make_unique<MockMigrationCoordinator>(browser()->profile());
  {
    testing::InSequence s;
    EXPECT_CALL(*coordinator.get(), Run(CloudProvider::kOneDrive, testing::_,
                                        testing::_, testing::_))
        .Times(1);
    EXPECT_CALL(*coordinator.get(), Stop).Times(1);
  }

  manager_->SetCoordinatorForTesting(std::move(coordinator));

  // Enable migration to OneDrive.
  SetMigrationPolicies(/*local_user_files_allowed=*/false,
                       /*destination=*/download_dir_util::kLocationOneDrive);
  task_runner->FastForwardBy(base::TimeDelta(base::Hours(24)));
  // Set migration to "read_only": stops the migration.
  SetMigrationPolicies(/*local_user_files_allowed=*/false,
                       /*destination=*/kReadOnly);
  task_runner->FastForwardBy(base::TimeDelta(base::Hours(24)));
}

INSTANTIATE_TEST_SUITE_P(
    LocalUserFiles,
    LocalFilesMigrationManagerLocationTest,
    ::testing::Values(download_dir_util::kLocationGoogleDrive,
                      download_dir_util::kLocationOneDrive),
    LocalFilesMigrationManagerLocationTest::ParamToName);

}  // namespace policy::local_user_files
