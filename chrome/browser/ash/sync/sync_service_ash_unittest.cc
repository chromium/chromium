// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/sync/sync_service_ash.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/sync/base/features.h"
#include "components/sync/driver/mock_sync_service.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

class SyncServiceAshTest : public testing::Test {
 public:
  SyncServiceAshTest() {
    override_features_.InitAndEnableFeature(
        syncer::kSyncChromeOSExplicitPassphraseSharing);
    sync_service_ash_ = std::make_unique<SyncServiceAsh>(&sync_service_);
  }

  SyncServiceAshTest(const SyncServiceAshTest&) = delete;
  SyncServiceAshTest& operator=(const SyncServiceAshTest&) = delete;
  ~SyncServiceAshTest() override = default;

  SyncServiceAsh* sync_service_ash() { return sync_service_ash_.get(); }

  void RunAllPendingTasks() { task_environment_.RunUntilIdle(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::test::ScopedFeatureList override_features_;

  testing::NiceMock<syncer::MockSyncService> sync_service_;
  std::unique_ptr<SyncServiceAsh> sync_service_ash_;
};

TEST_F(SyncServiceAshTest, ShouldSupportMultipleRemotes) {
  mojo::Remote<crosapi::mojom::SyncService> remote1;
  sync_service_ash()->BindReceiver(remote1.BindNewPipeAndPassReceiver());

  mojo::Remote<crosapi::mojom::SyncService> remote2;
  sync_service_ash()->BindReceiver(remote2.BindNewPipeAndPassReceiver());

  // Disconnect handlers are not called synchronously. They shouldn't be called
  // in this test, but to verify that wait for all pending tasks to be
  // completed.
  RunAllPendingTasks();
  EXPECT_TRUE(remote1.is_connected());
  EXPECT_TRUE(remote2.is_connected());
}

TEST_F(SyncServiceAshTest, ShouldDisconnectOnShutdown) {
  mojo::Remote<crosapi::mojom::SyncService> sync_service_ash_remote;
  sync_service_ash()->BindReceiver(
      sync_service_ash_remote.BindNewPipeAndPassReceiver());

  mojo::Remote<crosapi::mojom::SyncExplicitPassphraseClient>
      explicit_passphrase_client_remote;
  sync_service_ash()->BindExplicitPassphraseClient(
      explicit_passphrase_client_remote.BindNewPipeAndPassReceiver());

  ASSERT_TRUE(sync_service_ash_remote.is_connected());
  ASSERT_TRUE(explicit_passphrase_client_remote.is_connected());

  sync_service_ash()->Shutdown();
  // Wait for the disconnect handler to be called.
  RunAllPendingTasks();
  EXPECT_FALSE(sync_service_ash_remote.is_connected());
  EXPECT_FALSE(explicit_passphrase_client_remote.is_connected());
}

}  // namespace
}  // namespace ash
