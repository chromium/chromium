// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync_file_system/file_status_observer.h"
#include "chrome/browser/sync_file_system/local_change_processor.h"
#include "chrome/browser/sync_file_system/mock_remote_file_sync_service.h"
#include "chrome/browser/sync_file_system/sync_file_system_service.h"
#include "chrome/browser/sync_file_system/sync_file_system_service_factory.h"
#include "chrome/browser/sync_file_system/sync_status_code.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_function.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/quota/quota_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using storage::FileSystemURL;
using sync_file_system::MockRemoteFileSyncService;
using sync_file_system::RemoteFileSyncService;
using sync_file_system::SyncFileSystemServiceFactory;
using ::testing::_;
using ::testing::Invoke;
using ::testing::WithArg;

namespace {

enum class SyncActionMetrics {
  kNone = 0,
  kAdded = 1,
  kUpdated = 2,
  kDeleted = 3,
  kMaxValue = kDeleted
};

class SyncFileSystemApiTest : public extensions::ExtensionApiTest {
 public:
  SyncFileSystemApiTest() = default;

  void SetUpInProcessBrowserTestFixture() override {
    extensions::ExtensionApiTest::SetUpInProcessBrowserTestFixture();

    // TODO(calvinlo): Update test code after default quota is made const
    // (http://crbug.com/155488).
    real_default_quota_ =
        storage::QuotaManager::kSyncableStorageDefaultStorageKeyQuota;
    storage::QuotaManager::kSyncableStorageDefaultStorageKeyQuota = 123456;
  }

  void TearDownInProcessBrowserTestFixture() override {
    storage::QuotaManager::kSyncableStorageDefaultStorageKeyQuota =
        real_default_quota_;
    extensions::ExtensionApiTest::TearDownInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();

    // Override factory to inject a mock RemoteFileSyncService.
    // Must happen after the browser process is created because instantiating
    // the factory will instantiate ExtensionSystemFactory which depends on
    // ExtensionsBrowserClient setup in BrowserProcessImpl.
    SyncFileSystemServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(),
        base::BindLambdaForTesting([this](content::BrowserContext* context)
                                       -> std::unique_ptr<KeyedService> {
          auto remote_service = std::make_unique<
              ::testing::NiceMock<MockRemoteFileSyncService>>();
          mock_remote_service_ = remote_service.get();
          return SyncFileSystemServiceFactory::
              BuildWithRemoteFileSyncServiceForTest(context,
                                                    std::move(remote_service));
        }));
  }

  ::testing::NiceMock<MockRemoteFileSyncService>* mock_remote_service() {
    return mock_remote_service_;
  }

  const base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  raw_ptr<::testing::NiceMock<MockRemoteFileSyncService>, DanglingUntriaged>
      mock_remote_service_ = nullptr;
  int64_t real_default_quota_ = 0;
  base::HistogramTester histogram_tester_;
};

ACTION_P2(UpdateRemoteChangeQueue, origin, mock_remote_service) {
  *origin = arg0;
  mock_remote_service->NotifyRemoteChangeQueueUpdated(1);
}

// This functor is needed instead of using an ACTION_P6 because gmock
// unfortunately does not have good forwarding support for move-only types
// (|callback| in this case).
struct ReturnWithFakeFileAddedStatusFunctor {
  ReturnWithFakeFileAddedStatusFunctor(
      GURL* origin,
      MockRemoteFileSyncService* mock_remote_service,
      sync_file_system::SyncFileType file_type,
      sync_file_system::SyncFileStatus sync_file_status,
      sync_file_system::SyncAction sync_action_taken,
      sync_file_system::SyncDirection sync_direction)
      : origin_(origin),
        mock_remote_service_(mock_remote_service),
        file_type_(file_type),
        sync_file_status_(sync_file_status),
        sync_action_taken_(sync_action_taken),
        sync_direction_(sync_direction) {}

  void operator()(sync_file_system::SyncFileCallback callback) {
    FileSystemURL mock_url = sync_file_system::CreateSyncableFileSystemURL(
        *origin_, base::FilePath(FILE_PATH_LITERAL("foo.txt")));
    mock_remote_service_->NotifyRemoteChangeQueueUpdated(0);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  sync_file_system::SYNC_STATUS_OK, mock_url));
    mock_remote_service_->NotifyFileStatusChanged(
        mock_url, file_type_, sync_file_status_, sync_action_taken_,
        sync_direction_);
  }

 private:
  raw_ptr<GURL> origin_;
  raw_ptr<MockRemoteFileSyncService, DanglingUntriaged> mock_remote_service_;
  sync_file_system::SyncFileType file_type_;
  sync_file_system::SyncFileStatus sync_file_status_;
  sync_file_system::SyncAction sync_action_taken_;
  sync_file_system::SyncDirection sync_direction_;
};

}  // namespace

// Flaky on Win, OS X, and Linux: http://crbug.com/417330.
IN_PROC_BROWSER_TEST_F(SyncFileSystemApiTest, DISABLED_GetFileStatus) {
  ASSERT_TRUE(RunExtensionTest("sync_file_system/get_file_status",
                               {.launch_as_platform_app = true}))
      << message_;
}

// http://crbug.com/417330
IN_PROC_BROWSER_TEST_F(SyncFileSystemApiTest, DISABLED_GetFileStatuses) {
  // Mocking to return IsConflicting() == true only for the path "Conflicting".
  base::FilePath conflicting = base::FilePath::FromUTF8Unsafe("Conflicting");
  ASSERT_TRUE(RunExtensionTest("sync_file_system/get_file_statuses",
                               {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(SyncFileSystemApiTest, GetUsageAndQuota) {
  ASSERT_TRUE(RunExtensionTest("sync_file_system/get_usage_and_quota"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(SyncFileSystemApiTest, OnFileStatusChanged) {
  // Mock a pending remote change to be synced.
  // We ignore the did_respond trait on ExtensionFunction because we mock out
  // the service, which results in the callback never being called. Yuck.
  base::AutoReset<bool> ignore_did_respond(
      &ExtensionFunction::ignore_all_did_respond_for_testing_do_not_use, true);
  GURL origin;
  EXPECT_CALL(*mock_remote_service(), RegisterOrigin(_, _))
      .WillOnce(UpdateRemoteChangeQueue(&origin, mock_remote_service()));
  EXPECT_CALL(*mock_remote_service(), ProcessRemoteChange(_))
      .WillOnce(WithArg<0>(Invoke(ReturnWithFakeFileAddedStatusFunctor(
          &origin, mock_remote_service(), sync_file_system::SYNC_FILE_TYPE_FILE,
          sync_file_system::SYNC_FILE_STATUS_SYNCED,
          sync_file_system::SYNC_ACTION_ADDED,
          sync_file_system::SYNC_DIRECTION_REMOTE_TO_LOCAL))));
  ASSERT_TRUE(RunExtensionTest("sync_file_system/on_file_status_changed",
                               {.launch_as_platform_app = true}))
      << message_;
  histogram_tester().ExpectUniqueSample("Storage.SyncFileSystem.FileSyncAction",
                                        SyncActionMetrics::kAdded, 1);
}

IN_PROC_BROWSER_TEST_F(SyncFileSystemApiTest, OnFileStatusChangedDeleted) {
  // Mock a pending remote change to be synced.
  // We ignore the did_respond trait on ExtensionFunction because we mock out
  // the service, which results in the callback never being called. Yuck.
  base::AutoReset<bool> ignore_did_respond(
      &ExtensionFunction::ignore_all_did_respond_for_testing_do_not_use, true);
  GURL origin;
  EXPECT_CALL(*mock_remote_service(), RegisterOrigin(_, _))
      .WillOnce(UpdateRemoteChangeQueue(&origin, mock_remote_service()));
  EXPECT_CALL(*mock_remote_service(), ProcessRemoteChange(_))
      .WillOnce(WithArg<0>(Invoke(ReturnWithFakeFileAddedStatusFunctor(
          &origin, mock_remote_service(), sync_file_system::SYNC_FILE_TYPE_FILE,
          sync_file_system::SYNC_FILE_STATUS_SYNCED,
          sync_file_system::SYNC_ACTION_DELETED,
          sync_file_system::SYNC_DIRECTION_REMOTE_TO_LOCAL))));
  ASSERT_TRUE(
      RunExtensionTest("sync_file_system/on_file_status_changed_deleted",
                       {.launch_as_platform_app = true}))
      << message_;
  histogram_tester().ExpectUniqueSample("Storage.SyncFileSystem.FileSyncAction",
                                        SyncActionMetrics::kDeleted, 1);
}

IN_PROC_BROWSER_TEST_F(SyncFileSystemApiTest, OnServiceStatusChanged) {
  EXPECT_CALL(*mock_remote_service(), RegisterOrigin(_, _))
      .WillOnce([this](::testing::Unused,
                       sync_file_system::SyncStatusCallback callback) {
        mock_remote_service()->NotifyRemoteServiceStateUpdated(
            sync_file_system::REMOTE_SERVICE_OK, "Test event description.");
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback),
                                      sync_file_system::SYNC_STATUS_OK));
      });

  ASSERT_TRUE(RunExtensionTest("sync_file_system/on_service_status_changed",
                               {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(SyncFileSystemApiTest, RequestFileSystem) {
  EXPECT_CALL(*mock_remote_service(), RegisterOrigin(_, _)).Times(1);
  ASSERT_TRUE(RunExtensionTest("sync_file_system/request_file_system",
                               {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(SyncFileSystemApiTest, WriteFileThenGetUsage) {
  ASSERT_TRUE(RunExtensionTest("sync_file_system/write_file_then_get_usage",
                               {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(SyncFileSystemApiTest, ConflictResolutionPolicy) {
  ASSERT_TRUE(RunExtensionTest("sync_file_system/conflict_resolution_policy",
                               {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(SyncFileSystemApiTest, GetServiceStatus) {
  mock_remote_service()->SetServiceState(
      sync_file_system::REMOTE_SERVICE_AUTHENTICATION_REQUIRED);
  ASSERT_TRUE(RunExtensionTest("sync_file_system/get_service_status",
                               {.launch_as_platform_app = true}))
      << message_;
}
