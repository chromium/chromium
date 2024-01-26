// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/sync/sync_user_settings_client_ash.h"

#include <optional>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/crosapi/mojom/sync.mojom.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/test/mock_sync_service.h"
#include "components/sync/test/sync_user_settings_mock.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

using testing::Eq;
using testing::Ne;
using testing::Return;

class TestSyncUserSettingsClientObserver
    : public crosapi::mojom::SyncUserSettingsClientObserver {
 public:
  TestSyncUserSettingsClientObserver() = default;
  TestSyncUserSettingsClientObserver(
      const TestSyncUserSettingsClientObserver&) = delete;
  TestSyncUserSettingsClientObserver& operator=(
      const TestSyncUserSettingsClientObserver&) = delete;
  ~TestSyncUserSettingsClientObserver() override = default;

  void Observe(SyncUserSettingsClientAsh* client) {
    auto remote = receiver_.BindNewPipeAndPassRemote();
    client->AddObserver(std::move(remote));
  }

  std::optional<bool> GetLastAppsSyncEnabled() const {
    return last_apps_sync_enabled_;
  }

  // crosapi::mojom::SyncUserSettingsClientObserver implementation.
  void OnAppsSyncEnabledChanged(bool enabled) override {
    last_apps_sync_enabled_ = enabled;
  }

 private:
  std::optional<bool> last_apps_sync_enabled_;

  mojo::Receiver<crosapi::mojom::SyncUserSettingsClientObserver> receiver_{
      this};
};

class SyncUserSettingsClientAshTest : public testing::Test {
 public:
  SyncUserSettingsClientAshTest() {}
  SyncUserSettingsClientAshTest(const SyncUserSettingsClientAshTest&) = delete;
  SyncUserSettingsClientAshTest& operator=(
      const SyncUserSettingsClientAshTest&) = delete;
  ~SyncUserSettingsClientAshTest() override = default;

  void SetupClient() {
    client_ = std::make_unique<SyncUserSettingsClientAsh>(&sync_service_);
    client_->BindReceiver(client_remote_.BindNewPipeAndPassReceiver());
  }

  SyncUserSettingsClientAsh* client() {
    DCHECK(client_);
    return client_.get();
  }

  syncer::MockSyncService* sync_service() { return &sync_service_; }

  syncer::SyncUserSettingsMock* sync_user_settings() {
    return sync_service_.GetMockUserSettings();
  }

  bool IsAppsSyncEnabled() const {
    return IsAppsSyncEnabled(client_remote_.get());
  }

  bool IsAppsSyncEnabled(crosapi::mojom::SyncUserSettingsClient* client) const {
    base::test::TestFuture<bool> future;
    client->IsAppsSyncEnabled(future.GetCallback());
    return future.Take();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  testing::NiceMock<syncer::MockSyncService> sync_service_;

  std::unique_ptr<SyncUserSettingsClientAsh> client_;
  mojo::Remote<crosapi::mojom::SyncUserSettingsClient> client_remote_;
};

TEST_F(SyncUserSettingsClientAshTest, ShouldExposeAppsSyncEnabled) {
  ON_CALL(*sync_user_settings(), GetSelectedOsTypes())
      .WillByDefault(Return(syncer::UserSelectableOsTypeSet(
          {syncer::UserSelectableOsType::kOsApps})));
  SetupClient();

  EXPECT_TRUE(IsAppsSyncEnabled());
}

TEST_F(SyncUserSettingsClientAshTest, ShouldExposeAppsSyncDisabled) {
  ON_CALL(*sync_user_settings(), GetSelectedOsTypes())
      .WillByDefault(Return(syncer::UserSelectableOsTypeSet()));
  SetupClient();

  EXPECT_FALSE(IsAppsSyncEnabled());
}

TEST_F(SyncUserSettingsClientAshTest, ShouldSupportMultipleReceivers) {
  ON_CALL(*sync_user_settings(), GetSelectedOsTypes())
      .WillByDefault(Return(syncer::UserSelectableOsTypeSet(
          {syncer::UserSelectableOsType::kOsApps})));
  SetupClient();

  mojo::Remote<crosapi::mojom::SyncUserSettingsClient> other_remote;
  client()->BindReceiver(other_remote.BindNewPipeAndPassReceiver());

  EXPECT_TRUE(IsAppsSyncEnabled());
  EXPECT_TRUE(IsAppsSyncEnabled(other_remote.get()));
}

TEST_F(SyncUserSettingsClientAshTest, ShouldNotifyObserver) {
  ON_CALL(*sync_user_settings(), GetSelectedOsTypes())
      .WillByDefault(Return(syncer::UserSelectableOsTypeSet()));
  SetupClient();

  TestSyncUserSettingsClientObserver observer;
  observer.Observe(client());

  // No state changes, observer shouldn't be notified.
  client()->OnStateChanged(sync_service());
  client()->FlushMojoForTesting();
  EXPECT_THAT(observer.GetLastAppsSyncEnabled(), Eq(std::nullopt));

  // Mimic apps sync being enabled.
  ON_CALL(*sync_user_settings(), GetSelectedOsTypes())
      .WillByDefault(Return(syncer::UserSelectableOsTypeSet(
          {syncer::UserSelectableOsType::kOsApps})));
  client()->OnStateChanged(sync_service());
  client()->FlushMojoForTesting();
  ASSERT_THAT(observer.GetLastAppsSyncEnabled(), Ne(std::nullopt));
  EXPECT_TRUE(*observer.GetLastAppsSyncEnabled());

  // Mimic apps sync being disabled again.
  ON_CALL(*sync_user_settings(), GetSelectedOsTypes())
      .WillByDefault(Return(syncer::UserSelectableOsTypeSet()));
  client()->OnStateChanged(sync_service());
  client()->FlushMojoForTesting();
  ASSERT_THAT(observer.GetLastAppsSyncEnabled(), Ne(std::nullopt));
  EXPECT_FALSE(*observer.GetLastAppsSyncEnabled());
}

TEST_F(SyncUserSettingsClientAshTest, ShouldSupportMultipleObservers) {
  ON_CALL(*sync_user_settings(), GetSelectedOsTypes())
      .WillByDefault(Return(syncer::UserSelectableOsTypeSet()));
  SetupClient();

  TestSyncUserSettingsClientObserver observer1;
  observer1.Observe(client());

  TestSyncUserSettingsClientObserver observer2;
  observer2.Observe(client());

  // Mimic apps sync being enabled.
  ON_CALL(*sync_user_settings(), GetSelectedOsTypes())
      .WillByDefault(Return(syncer::UserSelectableOsTypeSet(
          {syncer::UserSelectableOsType::kOsApps})));
  client()->OnStateChanged(sync_service());
  client()->FlushMojoForTesting();

  ASSERT_THAT(observer1.GetLastAppsSyncEnabled(), Ne(std::nullopt));
  EXPECT_TRUE(*observer1.GetLastAppsSyncEnabled());

  ASSERT_THAT(observer2.GetLastAppsSyncEnabled(), Ne(std::nullopt));
  EXPECT_TRUE(*observer2.GetLastAppsSyncEnabled());
}

}  // namespace

}  // namespace ash
