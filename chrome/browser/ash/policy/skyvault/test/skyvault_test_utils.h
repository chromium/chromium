// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_SKYVAULT_TEST_SKYVAULT_TEST_UTILS_H_
#define CHROME_BROWSER_ASH_POLICY_SKYVAULT_TEST_SKYVAULT_TEST_UTILS_H_

#include "base/test/gmock_callback_support.h"
#include "chrome/browser/ash/policy/skyvault/local_files_migration_manager.h"
#include "chrome/browser/ash/policy/skyvault/migration_coordinator.h"
#include "chrome/browser/ash/policy/skyvault/migration_notification_manager.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace policy::local_user_files {
namespace {
constexpr char kEmail[] = "stub-user@example.com";
}

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

// Mock implementation of LocalFilesMigrationManager::Observer.
class MockMigrationObserver : public LocalFilesMigrationManager::Observer {
 public:
  MockMigrationObserver();
  ~MockMigrationObserver();

  MOCK_METHOD(void, OnMigrationSucceeded, (), (override));
};

// Mock implementation of MigrationNotificationManager.
class MockMigrationNotificationManager : public MigrationNotificationManager {
 public:
  explicit MockMigrationNotificationManager(content::BrowserContext* context);
  ~MockMigrationNotificationManager() override;

  MOCK_METHOD(void,
              ShowMigrationInfoDialog,
              (CloudProvider, base::Time, base::OnceClosure),
              (override));

  MOCK_METHOD(void,
              ShowConfigurationErrorNotification,
              (CloudProvider),
              (override));
};

// Mock implementation of MigrationCoordinator, with the default behavior to
// succeed the upload with a small delay.
class MockMigrationCoordinator : public MigrationCoordinator {
 public:
  explicit MockMigrationCoordinator(Profile* profile);
  ~MockMigrationCoordinator() override;

  // MigrationCoordinator overrides:
  bool IsRunning() const override { return is_running_; }
  void OnMigrationDone(
      MigrationDoneCallback callback,
      std::map<base::FilePath, MigrationUploadError> errors) override;

  // By default waits some minutes and completes the upload successfully.
  MOCK_METHOD(void,
              Run,
              (CloudProvider cloud_provider,
               std::vector<base::FilePath> file_paths,
               const std::string& destination_dir,
               MigrationDoneCallback callback),
              (override));
  MOCK_METHOD(void, Stop, (), (override));

  // Sets a callback to be invoked when Run() is called.
  void SetRunCallback(base::OnceClosure run_cb);

 private:
  bool is_running_ = false;
  // If set, invoked when Run() is.
  base::OnceClosure run_cb_;

  base::WeakPtrFactory<MockMigrationCoordinator> weak_ptr_factory_{this};
};

}  // namespace policy::local_user_files

#endif  // CHROME_BROWSER_ASH_POLICY_SKYVAULT_TEST_SKYVAULT_TEST_UTILS_H_
