// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/sync/sync_user_settings_client_lacros.h"

#include <utility>

#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "chromeos/crosapi/mojom/sync.mojom.h"
#include "components/sync/test/fake_sync_mojo_service.h"
#include "components/sync/test/fake_sync_user_settings_client_ash.h"
#include "components/sync/test/sync_user_settings_mock.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class SyncUserSettingsClientLacrosTest : public testing::Test {
 public:
  SyncUserSettingsClientLacrosTest() = default;
  ~SyncUserSettingsClientLacrosTest() override = default;

  SyncUserSettingsClientLacros CreateClientLacros() {
    mojo::Remote<crosapi::mojom::SyncUserSettingsClient> remote;
    client_ash_.BindReceiver(remote.BindNewPipeAndPassReceiver());
    return SyncUserSettingsClientLacros(std::move(remote),
                                        &sync_user_settings_);
  }

  syncer::SyncUserSettingsMock& user_settings() { return sync_user_settings_; }

  syncer::FakeSyncUserSettingsClientAsh& client_ash() { return client_ash_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  testing::NiceMock<syncer::SyncUserSettingsMock> sync_user_settings_;

  syncer::FakeSyncUserSettingsClientAsh client_ash_;
};

TEST_F(SyncUserSettingsClientLacrosTest,
       ShouldPlumbInitialAppsSyncEnabledValue) {
  base::RunLoop run_loop;
  EXPECT_CALL(
      user_settings(),
      SetAppsSyncEnabledByOs(
          syncer::FakeSyncUserSettingsClientAsh::kDefaultAppsSyncIsEnabled))
      .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));
  SyncUserSettingsClientLacros client_lacros = CreateClientLacros();
  run_loop.Run();
}

TEST_F(SyncUserSettingsClientLacrosTest, ShouldHandleAppsSyncEnabledChanges) {
  SyncUserSettingsClientLacros client_lacros = CreateClientLacros();
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
