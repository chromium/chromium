// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/sync/sync_user_settings_client_ash.h"

#include "base/test/task_environment.h"
#include "chromeos/crosapi/mojom/sync.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/sync.mojom.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/test/mock_sync_service.h"
#include "components/sync/test/sync_user_settings_mock.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

  absl::optional<bool> GetLastAppsSyncEnabled() const {
    return last_apps_sync_enabled_;
  }

  // crosapi::mojom::SyncUserSettingsClientObserver implementation.
  void OnAppsSyncEnabledChanged(bool enabled) override {
    last_apps_sync_enabled_ = enabled;
  }

 private:
  absl::optional<bool> last_apps_sync_enabled_;

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
    client_async_waiter_ =
        std::make_unique<crosapi::mojom::SyncUserSettingsClientAsyncWaiter>(
            client_remote_.get());
  }

  SyncUserSettingsClientAsh* client() {
    DCHECK(client_);
    return client_.get();
  }

  mojo::Remote<crosapi::mojom::SyncUserSettingsClient>* client_remote() {
    return &client_remote_;
  }

  crosapi::mojom::SyncUserSettingsClientAsyncWaiter* client_async_waiter() {
    DCHECK(client_async_waiter_);
    return client_async_waiter_.get();
  }

  syncer::MockSyncService* sync_service() { return &sync_service_; }

  syncer::SyncUserSettingsMock* sync_user_settings() {
    return sync_service_.GetMockUserSettings();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  testing::NiceMock<syncer::MockSyncService> sync_service_;

  std::unique_ptr<SyncUserSettingsClientAsh> client_;
  mojo::Remote<crosapi::mojom::SyncUserSettingsClient> client_remote_;
  std::unique_ptr<crosapi::mojom::SyncUserSettingsClientAsyncWaiter>
      client_async_waiter_;
};

TEST_F(SyncUserSettingsClientAshTest, ShouldExposeAppsSyncEnabled) {
  ON_CALL(*sync_user_settings(), GetSelectedOsTypes())
      .WillByDefault(Return(syncer::UserSelectableOsTypeSet(
          {syncer::UserSelectableOsType::kOsApps})));
  SetupClient();

  bool is_apps_sync_enabled = false;
  client_async_waiter()->IsAppsSyncEnabled(&is_apps_sync_enabled);
  EXPECT_TRUE(is_apps_sync_enabled);
}

TEST_F(SyncUserSettingsClientAshTest, ShouldExposeAppsSyncDisabled) {
  ON_CALL(*sync_user_settings(), GetSelectedOsTypes())
      .WillByDefault(Return(syncer::UserSelectableOsTypeSet()));
  SetupClient();

  bool is_apps_sync_enabled = false;
  client_async_waiter()->IsAppsSyncEnabled(&is_apps_sync_enabled);
  EXPECT_FALSE(is_apps_sync_enabled);
}

TEST_F(SyncUserSettingsClientAshTest, ShouldSupportMultipleReceivers) {
  ON_CALL(*sync_user_settings(), GetSelectedOsTypes())
      .WillByDefault(Return(syncer::UserSelectableOsTypeSet(
          {syncer::UserSelectableOsType::kOsApps})));
  SetupClient();

  mojo::Remote<crosapi::mojom::SyncUserSettingsClient> other_remote;
  client()->BindReceiver(other_remote.BindNewPipeAndPassReceiver());
  crosapi::mojom::SyncUserSettingsClientAsyncWaiter other_async_waiter(
      other_remote.get());

  bool is_apps_sync_enabled1 = false;
  client_async_waiter()->IsAppsSyncEnabled(&is_apps_sync_enabled1);
  EXPECT_TRUE(is_apps_sync_enabled1);

  bool is_apps_sync_enabled2 = false;
  other_async_waiter.IsAppsSyncEnabled(&is_apps_sync_enabled2);
  EXPECT_TRUE(is_apps_sync_enabled2);
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
  EXPECT_THAT(observer.GetLastAppsSyncEnabled(), Eq(absl::nullopt));

  // Mimic apps sync being enabled.
  ON_CALL(*sync_user_settings(), GetSelectedOsTypes())
      .WillByDefault(Return(syncer::UserSelectableOsTypeSet(
          {syncer::UserSelectableOsType::kOsApps})));
  client()->OnStateChanged(sync_service());
  client()->FlushMojoForTesting();
  ASSERT_THAT(observer.GetLastAppsSyncEnabled(), Ne(absl::nullopt));
  EXPECT_TRUE(*observer.GetLastAppsSyncEnabled());

  // Mimic apps sync being disabled again.
  ON_CALL(*sync_user_settings(), GetSelectedOsTypes())
      .WillByDefault(Return(syncer::UserSelectableOsTypeSet()));
  client()->OnStateChanged(sync_service());
  client()->FlushMojoForTesting();
  ASSERT_THAT(observer.GetLastAppsSyncEnabled(), Ne(absl::nullopt));
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

  ASSERT_THAT(observer1.GetLastAppsSyncEnabled(), Ne(absl::nullopt));
  EXPECT_TRUE(*observer1.GetLastAppsSyncEnabled());

  ASSERT_THAT(observer2.GetLastAppsSyncEnabled(), Ne(absl::nullopt));
  EXPECT_TRUE(*observer2.GetLastAppsSyncEnabled());
}

}  // namespace

}  // namespace ash
