// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/sync/sync_mojo_service_ash.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/sync/base/features.h"
#include "components/sync/test/mock_sync_service.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

class SyncMojoServiceAshTest : public testing::Test {
 public:
  SyncMojoServiceAshTest() {
    override_features_.InitWithFeatures(
        /*enabled_features=*/{syncer::kSyncChromeOSExplicitPassphraseSharing,
                              syncer::kSyncChromeOSAppsToggleSharing,
                              syncer::kChromeOSSyncedSessionSharing},
        /*disabled_features=*/{});
    sync_mojo_service_ash_ =
        std::make_unique<SyncMojoServiceAsh>(&sync_service_);
  }

  SyncMojoServiceAshTest(const SyncMojoServiceAshTest&) = delete;
  SyncMojoServiceAshTest& operator=(const SyncMojoServiceAshTest&) = delete;
  ~SyncMojoServiceAshTest() override = default;

  SyncMojoServiceAsh* sync_mojo_service_ash() {
    return sync_mojo_service_ash_.get();
  }

  void RunAllPendingTasks() { task_environment_.RunUntilIdle(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::test::ScopedFeatureList override_features_;

  testing::NiceMock<syncer::MockSyncService> sync_service_;
  std::unique_ptr<SyncMojoServiceAsh> sync_mojo_service_ash_;
};

TEST_F(SyncMojoServiceAshTest, ShouldSupportMultipleRemotes) {
  mojo::Remote<crosapi::mojom::SyncService> remote1;
  sync_mojo_service_ash()->BindReceiver(remote1.BindNewPipeAndPassReceiver());

  mojo::Remote<crosapi::mojom::SyncService> remote2;
  sync_mojo_service_ash()->BindReceiver(remote2.BindNewPipeAndPassReceiver());

  // Disconnect handlers are not called synchronously. They shouldn't be called
  // in this test, but to verify that wait for all pending tasks to be
  // completed.
  RunAllPendingTasks();
  EXPECT_TRUE(remote1.is_connected());
  EXPECT_TRUE(remote2.is_connected());
}

TEST_F(SyncMojoServiceAshTest, ShouldDisconnectOnShutdown) {
  mojo::Remote<crosapi::mojom::SyncService> sync_mojo_service_ash_remote;
  sync_mojo_service_ash()->BindReceiver(
      sync_mojo_service_ash_remote.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(sync_mojo_service_ash_remote.is_connected());

  mojo::Remote<crosapi::mojom::SyncExplicitPassphraseClient>
      explicit_passphrase_client_remote;
  sync_mojo_service_ash()->BindExplicitPassphraseClient(
      explicit_passphrase_client_remote.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(explicit_passphrase_client_remote.is_connected());

  mojo::Remote<crosapi::mojom::SyncUserSettingsClient>
      user_settings_client_remote;
  sync_mojo_service_ash()->BindUserSettingsClient(
      user_settings_client_remote.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(user_settings_client_remote.is_connected());

  mojo::Remote<crosapi::mojom::SyncedSessionClient>
      synced_session_client_remote;
  sync_mojo_service_ash()->BindSyncedSessionClient(
      synced_session_client_remote.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(synced_session_client_remote.is_connected());

  sync_mojo_service_ash()->Shutdown();
  // Wait for the disconnect handler to be called.
  RunAllPendingTasks();
  EXPECT_FALSE(sync_mojo_service_ash_remote.is_connected());
  EXPECT_FALSE(explicit_passphrase_client_remote.is_connected());
  EXPECT_FALSE(user_settings_client_remote.is_connected());
  EXPECT_FALSE(synced_session_client_remote.is_connected());
}

}  // namespace
}  // namespace ash
