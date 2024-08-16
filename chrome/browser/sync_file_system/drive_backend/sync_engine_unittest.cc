// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/sync_file_system/drive_backend/sync_engine.h"

#include <stddef.h>

#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/sync_file_system/drive_backend/callback_helper.h"
#include "chrome/browser/sync_file_system/drive_backend/fake_sync_worker.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_worker_interface.h"
#include "chrome/browser/sync_file_system/remote_file_sync_service.h"
#include "chrome/browser/sync_file_system/sync_file_system_test_util.h"
#include "components/drive/drive_uploader.h"
#include "components/drive/service/fake_drive_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_file_system {
namespace drive_backend {

class SyncEngineTest : public testing::Test {
 public:
  typedef RemoteFileSyncService::OriginStatusMap RemoteOriginStatusMap;

  SyncEngineTest() {}

  SyncEngineTest(const SyncEngineTest&) = delete;
  SyncEngineTest& operator=(const SyncEngineTest&) = delete;

  ~SyncEngineTest() override {}

  void SetUp() override {
    ASSERT_TRUE(profile_dir_.CreateUniqueTempDir());

    std::unique_ptr<drive::DriveServiceInterface> fake_drive_service(
        new drive::FakeDriveService);

    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner =
        base::SingleThreadTaskRunner::GetCurrentDefault();
    worker_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

    sync_engine_.reset(new drive_backend::SyncEngine(
        ui_task_runner.get(), worker_task_runner_.get(),
        nullptr,  // drive_task_runner
        profile_dir_.GetPath(),
        nullptr,    // task_logger
        nullptr,    // notification_manager
        nullptr,    // extension_service
        nullptr,    // extension_registry
        nullptr,    // identity_manager
        nullptr,    // url_loader_factory
        nullptr,    // drive_service_factory
        nullptr));  // in_memory_env

    sync_engine_->InitializeForTesting(
        std::move(fake_drive_service),
        nullptr,  // drive_uploader
        std::unique_ptr<SyncWorkerInterface>(new FakeSyncWorker));
    sync_engine_->SetSyncEnabled(true);
    sync_engine_->OnReadyToSendRequests();

    WaitForWorkerTaskRunner();
  }

  void TearDown() override {
    sync_engine_.reset();
    WaitForWorkerTaskRunner();

    worker_task_runner_ = nullptr;
  }

  bool FindOriginStatus(const GURL& origin, std::string* status) {
    std::unique_ptr<RemoteOriginStatusMap> status_map;
    sync_engine()->GetOriginStatusMap(CreateResultReceiver(&status_map));
    WaitForWorkerTaskRunner();

    RemoteOriginStatusMap::const_iterator itr = status_map->find(origin);
    if (itr == status_map->end())
      return false;

    *status = itr->second;
    // If an origin is uninstalled, it should not be found actually.
    if (*status == "Uninstalled")
      return false;
    return true;
  }

  void PostUpdateServiceState(RemoteServiceState state,
                              const std::string& description) {
    worker_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&FakeSyncWorker::UpdateServiceState,
                                  base::Unretained(fake_sync_worker()), state,
                                  description));
    WaitForWorkerTaskRunner();
  }

  void WaitForWorkerTaskRunner() {
    DCHECK(worker_task_runner_.get());

    base::RunLoop run_loop;
    worker_task_runner_->PostTask(
        FROM_HERE,
        RelayCallbackToCurrentThread(
            FROM_HERE, run_loop.QuitClosure()));
    run_loop.Run();
  }

  // Accessors
  SyncEngine* sync_engine() { return sync_engine_.get(); }

  FakeSyncWorker* fake_sync_worker() {
    return static_cast<FakeSyncWorker*>(sync_engine_->sync_worker_.get());
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir profile_dir_;
  std::unique_ptr<drive_backend::SyncEngine> sync_engine_;

  scoped_refptr<base::SequencedTaskRunner> worker_task_runner_;
};

TEST_F(SyncEngineTest, OriginTest) {
  GURL origin("chrome-extension://app_0");

  SyncStatusCode sync_status;
  std::string status;

  sync_engine()->RegisterOrigin(
      origin,
      CreateResultReceiver(&sync_status));
  WaitForWorkerTaskRunner();
  EXPECT_EQ(SYNC_STATUS_OK, sync_status);
  ASSERT_TRUE(FindOriginStatus(origin, &status));
  EXPECT_EQ("Registered", status);

  sync_engine()->DisableOrigin(
      origin,
      CreateResultReceiver(&sync_status));
  WaitForWorkerTaskRunner();
  EXPECT_EQ(SYNC_STATUS_OK, sync_status);
  ASSERT_TRUE(FindOriginStatus(origin, &status));
  EXPECT_EQ("Disabled", status);

  sync_engine()->EnableOrigin(
      origin,
      CreateResultReceiver(&sync_status));
  WaitForWorkerTaskRunner();
  EXPECT_EQ(SYNC_STATUS_OK, sync_status);
  ASSERT_TRUE(FindOriginStatus(origin, &status));
  EXPECT_EQ("Enabled", status);

  sync_engine()->UninstallOrigin(
      origin,
      RemoteFileSyncService::UNINSTALL_AND_KEEP_REMOTE,
      CreateResultReceiver(&sync_status));
  WaitForWorkerTaskRunner();
  EXPECT_EQ(SYNC_STATUS_OK, sync_status);
  EXPECT_FALSE(FindOriginStatus(origin, &status));
  EXPECT_EQ("Uninstalled", status);
}

TEST_F(SyncEngineTest, GetOriginStatusMap) {
  SyncStatusCode sync_status = SYNC_STATUS_UNKNOWN;

  sync_engine()->RegisterOrigin(GURL("chrome-extension://app_0"),
                                CreateResultReceiver(&sync_status));
  WaitForWorkerTaskRunner();
  EXPECT_EQ(SYNC_STATUS_OK, sync_status);

  sync_engine()->RegisterOrigin(GURL("chrome-extension://app_1"),
                                CreateResultReceiver(&sync_status));
  WaitForWorkerTaskRunner();
  EXPECT_EQ(SYNC_STATUS_OK, sync_status);

  std::unique_ptr<RemoteOriginStatusMap> status_map;
  sync_engine()->GetOriginStatusMap(CreateResultReceiver(&status_map));
  WaitForWorkerTaskRunner();
  ASSERT_EQ(2u, status_map->size());
  EXPECT_EQ("Registered", (*status_map)[GURL("chrome-extension://app_0")]);
  EXPECT_EQ("Registered", (*status_map)[GURL("chrome-extension://app_1")]);

  sync_engine()->DisableOrigin(GURL("chrome-extension://app_1"),
                               CreateResultReceiver(&sync_status));
  WaitForWorkerTaskRunner();
  EXPECT_EQ(SYNC_STATUS_OK, sync_status);

  sync_engine()->GetOriginStatusMap(CreateResultReceiver(&status_map));
  WaitForWorkerTaskRunner();
  ASSERT_EQ(2u, status_map->size());
  EXPECT_EQ("Registered", (*status_map)[GURL("chrome-extension://app_0")]);
  EXPECT_EQ("Disabled", (*status_map)[GURL("chrome-extension://app_1")]);
}

TEST_F(SyncEngineTest, UpdateServiceState) {
  struct {
    RemoteServiceState state;
    const char* description;
  } test_data[] = {
    {REMOTE_SERVICE_OK, "OK"},
    {REMOTE_SERVICE_TEMPORARY_UNAVAILABLE, "TEMPORARY_UNAVAILABLE"},
    {REMOTE_SERVICE_AUTHENTICATION_REQUIRED, "AUTHENTICATION_REQUIRED"},
    {REMOTE_SERVICE_DISABLED, "DISABLED"},
  };

  for (size_t i = 0; i < std::size(test_data); ++i) {
    PostUpdateServiceState(test_data[i].state, test_data[i].description);
    EXPECT_EQ(test_data[i].state, sync_engine()->GetCurrentState())
        << "Expected state: REMOTE_SERVICE_" << test_data[i].description;
  }
}

TEST_F(SyncEngineTest, ProcessRemoteChange) {
  SyncStatusCode sync_status;
  storage::FileSystemURL url;
  sync_engine()->ProcessRemoteChange(CreateResultReceiver(&sync_status, &url));
  WaitForWorkerTaskRunner();
  EXPECT_EQ(SYNC_STATUS_OK, sync_status);
}

}  // namespace drive_backend
}  // namespace sync_file_system
