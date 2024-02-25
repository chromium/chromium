// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/sync_worker.h"

#include <memory>
#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "chrome/browser/extensions/test_extension_service.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_context.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task_manager.h"
#include "chrome/browser/sync_file_system/sync_file_system_test_util.h"
#include "components/drive/drive_uploader.h"
#include "components/drive/service/fake_drive_service.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/leveldb_chrome.h"

namespace sync_file_system {
namespace drive_backend {

namespace {

const char kAppID[] = "app_id";

void EmptyTask(SyncStatusCode status, SyncStatusCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), status));
}

}  // namespace

class MockSyncTask : public ExclusiveTask {
 public:
  explicit MockSyncTask(bool used_network) {
    set_used_network(used_network);
  }

  MockSyncTask(const MockSyncTask&) = delete;
  MockSyncTask& operator=(const MockSyncTask&) = delete;

  ~MockSyncTask() override {}

  void RunExclusive(SyncStatusCallback callback) override {
    std::move(callback).Run(SYNC_STATUS_OK);
  }
};

class MockExtensionService : public TestExtensionService {
 public:
  MockExtensionService() : registry_(nullptr) {}

  MockExtensionService(const MockExtensionService&) = delete;
  MockExtensionService& operator=(const MockExtensionService&) = delete;

  ~MockExtensionService() override {}

  void AddExtension(const extensions::Extension* extension) override {
    registry_.AddEnabled(base::WrapRefCounted(extension));
  }

  bool IsExtensionEnabled(const std::string& extension_id) const override {
    return registry_.enabled_extensions().Contains(extension_id);
  }

  void UninstallExtension(const std::string& extension_id) {
    EXPECT_TRUE(registry_.RemoveEnabled(extension_id) ||
                registry_.RemoveDisabled(extension_id));
  }

  void DisableExtension(const std::string& extension_id) {
    if (!IsExtensionEnabled(extension_id))
      return;
    scoped_refptr<const extensions::Extension> extension =
        registry_.GetInstalledExtension(extension_id);
    EXPECT_TRUE(registry_.RemoveEnabled(extension_id));
    registry_.AddDisabled(extension);
  }

  extensions::ExtensionRegistry& registry() { return registry_; }

 private:
  extensions::ExtensionRegistry registry_;
};

class SyncWorkerTest : public testing::Test {
 public:
  SyncWorkerTest() {}

  SyncWorkerTest(const SyncWorkerTest&) = delete;
  SyncWorkerTest& operator=(const SyncWorkerTest&) = delete;

  ~SyncWorkerTest() override {}

  void SetUp() override {
    ASSERT_TRUE(profile_dir_.CreateUniqueTempDir());
    in_memory_env_ = leveldb_chrome::NewMemEnv("SyncWorkerTest");

    extension_service_ = std::make_unique<MockExtensionService>();
    std::unique_ptr<drive::DriveServiceInterface> fake_drive_service(
        new drive::FakeDriveService);

    std::unique_ptr<SyncEngineContext> sync_engine_context(
        new SyncEngineContext(
            std::move(fake_drive_service), nullptr /* drive_uploader */,
            nullptr /* task_logger */,
            base::SingleThreadTaskRunner::
                GetCurrentDefault() /* ui_task_runner */,
            base::SingleThreadTaskRunner::
                GetCurrentDefault() /* worker_task_runner */));

    sync_worker_ = std::make_unique<SyncWorker>(
        profile_dir_.GetPath(), extension_service_->AsWeakPtr(),
        &extension_service_->registry(), in_memory_env_.get());
    sync_worker_->Initialize(std::move(sync_engine_context));

    sync_worker_->SetSyncEnabled(true);
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    sync_worker_.reset();
    extension_service_.reset();
    base::RunLoop().RunUntilIdle();
  }

  MockExtensionService* extension_service() { return extension_service_.get(); }
  SyncWorker* sync_worker() { return sync_worker_.get(); }

  void UpdateRegisteredApps() {
    sync_worker_->UpdateRegisteredApps();
  }

  SyncTaskManager* GetSyncTaskManager() {
    return sync_worker_->task_manager_.get();
  }

  void CheckServiceState(SyncStatusCode expected_sync_status,
                         RemoteServiceState expected_service_status,
                         SyncStatusCode sync_status) {
    EXPECT_EQ(expected_sync_status, sync_status);
    EXPECT_EQ(expected_service_status, sync_worker_->GetCurrentState());
  }

  MetadataDatabase* metadata_database() {
    return sync_worker_->GetMetadataDatabase();
  }

  base::WeakPtr<SyncWorkerTest> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir profile_dir_;
  std::unique_ptr<leveldb::Env> in_memory_env_;

  std::unique_ptr<MockExtensionService> extension_service_;
  std::unique_ptr<SyncWorker> sync_worker_;
  base::WeakPtrFactory<SyncWorkerTest> weak_ptr_factory_{this};
};

TEST_F(SyncWorkerTest, EnableOrigin) {
  FileTracker tracker;
  SyncStatusCode sync_status = SYNC_STATUS_UNKNOWN;
  GURL origin = extensions::Extension::GetBaseURLFromExtensionId(kAppID);

  sync_worker()->RegisterOrigin(origin, CreateResultReceiver(&sync_status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SYNC_STATUS_OK, sync_status);
  ASSERT_TRUE(metadata_database()->FindAppRootTracker(kAppID, &tracker));
  EXPECT_EQ(TRACKER_KIND_APP_ROOT, tracker.tracker_kind());

  sync_worker()->DisableOrigin(origin, CreateResultReceiver(&sync_status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SYNC_STATUS_OK, sync_status);
  ASSERT_TRUE(metadata_database()->FindAppRootTracker(kAppID, &tracker));
  EXPECT_EQ(TRACKER_KIND_DISABLED_APP_ROOT, tracker.tracker_kind());

  sync_worker()->EnableOrigin(origin, CreateResultReceiver(&sync_status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SYNC_STATUS_OK, sync_status);
  ASSERT_TRUE(metadata_database()->FindAppRootTracker(kAppID, &tracker));
  EXPECT_EQ(TRACKER_KIND_APP_ROOT, tracker.tracker_kind());

  sync_worker()->UninstallOrigin(
      origin,
      RemoteFileSyncService::UNINSTALL_AND_KEEP_REMOTE,
      CreateResultReceiver(&sync_status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SYNC_STATUS_OK, sync_status);
  ASSERT_FALSE(metadata_database()->FindAppRootTracker(kAppID, &tracker));
}

TEST_F(SyncWorkerTest, UpdateRegisteredApps) {
  SyncStatusCode sync_status = SYNC_STATUS_UNKNOWN;
  for (int i = 0; i < 3; i++) {
    scoped_refptr<const extensions::Extension> extension =
        extensions::ExtensionBuilder()
            .SetManifest(base::Value::Dict()
                             .Set("name", "foo")
                             .Set("version", "1.0")
                             .Set("manifest_version", 2))
            .SetID(base::StringPrintf("app_%d", i))
            .Build();
    extension_service()->AddExtension(extension.get());
    GURL origin = extensions::Extension::GetBaseURLFromExtensionId(
        extension->id());
    sync_status = SYNC_STATUS_UNKNOWN;
    sync_worker()->RegisterOrigin(origin, CreateResultReceiver(&sync_status));
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(SYNC_STATUS_OK, sync_status);
  }

  FileTracker tracker;

  ASSERT_TRUE(metadata_database()->FindAppRootTracker("app_0", &tracker));
  EXPECT_EQ(TRACKER_KIND_APP_ROOT, tracker.tracker_kind());

  ASSERT_TRUE(metadata_database()->FindAppRootTracker("app_1", &tracker));
  EXPECT_EQ(TRACKER_KIND_APP_ROOT, tracker.tracker_kind());

  ASSERT_TRUE(metadata_database()->FindAppRootTracker("app_2", &tracker));
  EXPECT_EQ(TRACKER_KIND_APP_ROOT, tracker.tracker_kind());

  extension_service()->DisableExtension("app_1");
  extension_service()->UninstallExtension("app_2");
  ASSERT_FALSE(extension_service()->registry().GetInstalledExtension("app_2"));
  UpdateRegisteredApps();
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(metadata_database()->FindAppRootTracker("app_0", &tracker));
  EXPECT_EQ(TRACKER_KIND_APP_ROOT, tracker.tracker_kind());

  ASSERT_TRUE(metadata_database()->FindAppRootTracker("app_1", &tracker));
  EXPECT_EQ(TRACKER_KIND_DISABLED_APP_ROOT, tracker.tracker_kind());

  ASSERT_FALSE(metadata_database()->FindAppRootTracker("app_2", &tracker));
}

TEST_F(SyncWorkerTest, GetOriginStatusMap) {
  FileTracker tracker;
  SyncStatusCode sync_status = SYNC_STATUS_UNKNOWN;
  GURL origin = extensions::Extension::GetBaseURLFromExtensionId(kAppID);

  sync_worker()->RegisterOrigin(GURL("chrome-extension://app_0"),
                                CreateResultReceiver(&sync_status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SYNC_STATUS_OK, sync_status);

  sync_worker()->RegisterOrigin(GURL("chrome-extension://app_1"),
                                CreateResultReceiver(&sync_status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SYNC_STATUS_OK, sync_status);

  std::unique_ptr<RemoteFileSyncService::OriginStatusMap> status_map;
  sync_worker()->GetOriginStatusMap(CreateResultReceiver(&status_map));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(2u, status_map->size());
  EXPECT_EQ("Enabled", (*status_map)[GURL("chrome-extension://app_0")]);
  EXPECT_EQ("Enabled", (*status_map)[GURL("chrome-extension://app_1")]);

  sync_worker()->DisableOrigin(GURL("chrome-extension://app_1"),
                               CreateResultReceiver(&sync_status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SYNC_STATUS_OK, sync_status);

  sync_worker()->GetOriginStatusMap(CreateResultReceiver(&status_map));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(2u, status_map->size());
  EXPECT_EQ("Enabled", (*status_map)[GURL("chrome-extension://app_0")]);
  EXPECT_EQ("Disabled", (*status_map)[GURL("chrome-extension://app_1")]);
}

TEST_F(SyncWorkerTest, UpdateServiceState) {
  EXPECT_EQ(REMOTE_SERVICE_OK, sync_worker()->GetCurrentState());

  GetSyncTaskManager()->ScheduleTask(
      FROM_HERE, base::BindOnce(&EmptyTask, SYNC_STATUS_AUTHENTICATION_FAILED),
      SyncTaskManager::PRIORITY_MED,
      base::BindOnce(&SyncWorkerTest::CheckServiceState, AsWeakPtr(),
                     SYNC_STATUS_AUTHENTICATION_FAILED,
                     REMOTE_SERVICE_AUTHENTICATION_REQUIRED));

  GetSyncTaskManager()->ScheduleTask(
      FROM_HERE, base::BindOnce(&EmptyTask, SYNC_STATUS_ACCESS_FORBIDDEN),
      SyncTaskManager::PRIORITY_MED,
      base::BindOnce(&SyncWorkerTest::CheckServiceState, AsWeakPtr(),
                     SYNC_STATUS_ACCESS_FORBIDDEN,
                     REMOTE_SERVICE_ACCESS_FORBIDDEN));

  GetSyncTaskManager()->ScheduleTask(
      FROM_HERE,
      base::BindOnce(&EmptyTask, SYNC_STATUS_SERVICE_TEMPORARILY_UNAVAILABLE),
      SyncTaskManager::PRIORITY_MED,
      base::BindOnce(&SyncWorkerTest::CheckServiceState, AsWeakPtr(),
                     SYNC_STATUS_SERVICE_TEMPORARILY_UNAVAILABLE,
                     REMOTE_SERVICE_TEMPORARY_UNAVAILABLE));

  GetSyncTaskManager()->ScheduleTask(
      FROM_HERE, base::BindOnce(&EmptyTask, SYNC_STATUS_NETWORK_ERROR),
      SyncTaskManager::PRIORITY_MED,
      base::BindOnce(&SyncWorkerTest::CheckServiceState, AsWeakPtr(),
                     SYNC_STATUS_NETWORK_ERROR,
                     REMOTE_SERVICE_TEMPORARY_UNAVAILABLE));

  GetSyncTaskManager()->ScheduleTask(
      FROM_HERE, base::BindOnce(&EmptyTask, SYNC_STATUS_ABORT),
      SyncTaskManager::PRIORITY_MED,
      base::BindOnce(&SyncWorkerTest::CheckServiceState, AsWeakPtr(),
                     SYNC_STATUS_ABORT, REMOTE_SERVICE_TEMPORARY_UNAVAILABLE));

  GetSyncTaskManager()->ScheduleTask(
      FROM_HERE, base::BindOnce(&EmptyTask, SYNC_STATUS_FAILED),
      SyncTaskManager::PRIORITY_MED,
      base::BindOnce(&SyncWorkerTest::CheckServiceState, AsWeakPtr(),
                     SYNC_STATUS_FAILED, REMOTE_SERVICE_TEMPORARY_UNAVAILABLE));

  GetSyncTaskManager()->ScheduleTask(
      FROM_HERE, base::BindOnce(&EmptyTask, SYNC_DATABASE_ERROR_CORRUPTION),
      SyncTaskManager::PRIORITY_MED,
      base::BindOnce(&SyncWorkerTest::CheckServiceState, AsWeakPtr(),
                     SYNC_DATABASE_ERROR_CORRUPTION, REMOTE_SERVICE_DISABLED));

  GetSyncTaskManager()->ScheduleTask(
      FROM_HERE, base::BindOnce(&EmptyTask, SYNC_DATABASE_ERROR_IO_ERROR),
      SyncTaskManager::PRIORITY_MED,
      base::BindOnce(&SyncWorkerTest::CheckServiceState, AsWeakPtr(),
                     SYNC_DATABASE_ERROR_IO_ERROR, REMOTE_SERVICE_DISABLED));

  GetSyncTaskManager()->ScheduleTask(
      FROM_HERE, base::BindOnce(&EmptyTask, SYNC_DATABASE_ERROR_FAILED),
      SyncTaskManager::PRIORITY_MED,
      base::BindOnce(&SyncWorkerTest::CheckServiceState, AsWeakPtr(),
                     SYNC_DATABASE_ERROR_FAILED, REMOTE_SERVICE_DISABLED));

  GetSyncTaskManager()->ScheduleSyncTask(
      FROM_HERE, std::unique_ptr<SyncTask>(new MockSyncTask(false)),
      SyncTaskManager::PRIORITY_MED,
      base::BindOnce(&SyncWorkerTest::CheckServiceState, AsWeakPtr(),
                     SYNC_STATUS_OK, REMOTE_SERVICE_DISABLED));

  GetSyncTaskManager()->ScheduleSyncTask(
      FROM_HERE, std::unique_ptr<SyncTask>(new MockSyncTask(true)),
      SyncTaskManager::PRIORITY_MED,
      base::BindOnce(&SyncWorkerTest::CheckServiceState, AsWeakPtr(),
                     SYNC_STATUS_OK, REMOTE_SERVICE_OK));

  base::RunLoop().RunUntilIdle();
}

}  // namespace drive_backend
}  // namespace sync_file_system
