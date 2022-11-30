// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/sync/sync_user_settings_client_lacros.h"

#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "chromeos/crosapi/mojom/sync.mojom.h"
#include "components/sync/test/fake_sync_mojo_service.h"
#include "components/sync/test/fake_sync_user_settings_client_ash.h"
#include "components/sync/test/mock_sync_service.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class SyncUserSettingsClientLacrosTest : public testing::Test {
 public:
  SyncUserSettingsClientLacrosTest() = default;
  ~SyncUserSettingsClientLacrosTest() override = default;

  void SetUp() override {
    sync_mojo_service_.BindReceiver(
        sync_mojo_service_remote_.BindNewPipeAndPassReceiver());
    client_lacros_ = std::make_unique<SyncUserSettingsClientLacros>(
        &sync_service_, &sync_mojo_service_remote_);
  }

  syncer::MockSyncService& sync_service() { return sync_service_; }

  syncer::SyncUserSettingsMock& user_settings() {
    return *sync_service_.GetMockUserSettings();
  }

  mojo::Remote<crosapi::mojom::SyncService>& sync_mojo_service_remote() {
    return sync_mojo_service_remote_;
  }

  syncer::FakeSyncUserSettingsClientAsh& client_ash() {
    return sync_mojo_service_.GetFakeSyncUserSettingsClientAsh();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  testing::NiceMock<syncer::MockSyncService> sync_service_;

  mojo::Remote<crosapi::mojom::SyncService> sync_mojo_service_remote_;
  syncer::FakeSyncMojoService sync_mojo_service_;

  std::unique_ptr<SyncUserSettingsClientLacros> client_lacros_;
};

TEST_F(SyncUserSettingsClientLacrosTest,
       ShouldPlumbInitialAppsSyncEnabledValue) {
  base::RunLoop run_loop;
  EXPECT_CALL(
      user_settings(),
      SetAppsSyncEnabledByOs(
          syncer::FakeSyncUserSettingsClientAsh::kDefaultAppsSyncIsEnabled))
      .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));
  SyncUserSettingsClientLacros client_lacros(&sync_service(),
                                             &sync_mojo_service_remote());
  run_loop.Run();
}

TEST_F(SyncUserSettingsClientLacrosTest, ShouldHandleAppsSyncEnabledChanges) {
  SyncUserSettingsClientLacros client_lacros(&sync_service(),
                                             &sync_mojo_service_remote());
  const bool kNonDefaultAppsSyncIsEnabled =
      !syncer::FakeSyncUserSettingsClientAsh::kDefaultAppsSyncIsEnabled;
  base::RunLoop run_loop;
  EXPECT_CALL(user_settings(),
              SetAppsSyncEnabledByOs(kNonDefaultAppsSyncIsEnabled))
      .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));
  client_ash().SetAppsSyncIsEnabled(kNonDefaultAppsSyncIsEnabled);
  run_loop.Run();
}

}  // namespace
