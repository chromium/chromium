// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/sync/sync_appsync_service.h"
#include <memory>

#include "base/test/task_environment.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "components/sync/test/mock_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

class SyncAppsyncServiceTest : public testing::Test {
 public:
  SyncAppsyncServiceTest() {
    sync_appsync_service_ =
        std::make_unique<SyncAppsyncService>(&sync_service_, &user_manager_);
  }

  SyncAppsyncServiceTest(const SyncAppsyncServiceTest&) = delete;
  SyncAppsyncServiceTest& operator=(const SyncAppsyncServiceTest&) = delete;
  ~SyncAppsyncServiceTest() override = default;

  SyncAppsyncService* sync_appsync_service() {
    return sync_appsync_service_.get();
  }

  void RunAllPendingTasks() { task_environment_.RunUntilIdle(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  testing::NiceMock<syncer::MockSyncService> sync_service_;
  FakeChromeUserManager user_manager_;
  std::unique_ptr<SyncAppsyncService> sync_appsync_service_;
};

TEST_F(SyncAppsyncServiceTest, DoesNotCrash) {
  sync_appsync_service()->Shutdown();
  RunAllPendingTasks();
}

}  // namespace

}  // namespace ash
