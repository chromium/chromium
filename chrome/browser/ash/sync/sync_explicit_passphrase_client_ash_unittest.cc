// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/sync/sync_explicit_passphrase_client_ash.h"

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/crosapi/mojom/sync.mojom-forward.h"
#include "chromeos/crosapi/mojom/sync.mojom-test-utils.h"
#include "components/sync/chromeos/explicit_passphrase_mojo_utils.h"
#include "components/sync/engine/nigori/key_derivation_params.h"
#include "components/sync/engine/nigori/nigori.h"
#include "components/sync/test/mock_sync_service.h"
#include "components/sync/test/sync_user_settings_mock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

using testing::Eq;
using testing::NotNull;
using testing::Return;

std::unique_ptr<syncer::Nigori> MakeTestNigoriKey() {
  return syncer::Nigori::CreateByDerivation(
      syncer::KeyDerivationParams::CreateForPbkdf2(), /*password=*/"password");
}

crosapi::mojom::NigoriKeyPtr MakeTestMojoNigoriKey() {
  std::unique_ptr<syncer::Nigori> nigori_key = MakeTestNigoriKey();
  return syncer::NigoriToMojo(*nigori_key);
}

class TestSyncExplicitPassphraseClientObserver
    : public crosapi::mojom::
          SyncExplicitPassphraseClientObserverInterceptorForTesting {
 public:
  TestSyncExplicitPassphraseClientObserver() = default;
  TestSyncExplicitPassphraseClientObserver(
      const TestSyncExplicitPassphraseClientObserver& other) = delete;
  TestSyncExplicitPassphraseClientObserver& operator=(
      const TestSyncExplicitPassphraseClientObserver& other) = delete;
  ~TestSyncExplicitPassphraseClientObserver() override = default;

  void Observe(SyncExplicitPassphraseClientAsh* client) {
    auto remote = receiver_.BindNewPipeAndPassRemote();
    client->AddObserver(std::move(remote));
  }

  int GetNumOnPassphraseRequiredCalls() const {
    return num_on_passphrase_required_calls_;
  }

  int GetNumOnPassphraseAvailableCalls() const {
    return num_on_passphrase_available_calls_;
  }

  // crosapi::mojom::SyncExplicitPassphraseClientObserverInterceptorForTesting
  // overrides:
  SyncExplicitPassphraseClientObserver* GetForwardingInterface() override {
    return this;
  }

  void OnPassphraseRequired() override { num_on_passphrase_required_calls_++; }

  void OnPassphraseAvailable() override {
    num_on_passphrase_available_calls_++;
  }

 private:
  int num_on_passphrase_required_calls_ = 0;
  int num_on_passphrase_available_calls_ = 0;

  mojo::Receiver<crosapi::mojom::SyncExplicitPassphraseClientObserver>
      receiver_{this};
};

class SyncExplicitPassphraseClientAshTest : public testing::Test {
 public:
  SyncExplicitPassphraseClientAshTest() : client_(&sync_service_) {
    sync_account_info_.gaia = "user1";
  }

  SyncExplicitPassphraseClientAshTest(
      const SyncExplicitPassphraseClientAshTest&) = delete;
  SyncExplicitPassphraseClientAshTest& operator=(
      const SyncExplicitPassphraseClientAshTest&) = delete;
  ~SyncExplicitPassphraseClientAshTest() override = default;

  void SetUp() override {
    ON_CALL(sync_service_, GetAccountInfo())
        .WillByDefault(Return(sync_account_info_));
    client_.BindReceiver(client_remote_.BindNewPipeAndPassReceiver());
  }

  SyncExplicitPassphraseClientAsh* client() { return &client_; }

  crosapi::mojom::NigoriKeyPtr GetDecryptionNigoriKey(
      crosapi::mojom::AccountKeyPtr key) const {
    base::test::TestFuture<crosapi::mojom::NigoriKeyPtr> future;
    client_remote_->GetDecryptionNigoriKey(std::move(key),
                                           future.GetCallback());
    return future.Take();
  }

  syncer::MockSyncService* sync_service() { return &sync_service_; }

  syncer::SyncUserSettingsMock* sync_user_settings() {
    return sync_service_.GetMockUserSettings();
  }

  const CoreAccountInfo& sync_account_info() const {
    return sync_account_info_;
  }

  crosapi::mojom::AccountKeyPtr GetSyncingAccountKey() const {
    crosapi::mojom::AccountKeyPtr account_key =
        crosapi::mojom::AccountKey::New();
    account_key->id = sync_account_info_.gaia;
    account_key->account_type = crosapi::mojom::AccountType::kGaia;
    return account_key;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  testing::NiceMock<syncer::MockSyncService> sync_service_;

  SyncExplicitPassphraseClientAsh client_;
  mojo::Remote<crosapi::mojom::SyncExplicitPassphraseClient> client_remote_;

  CoreAccountInfo sync_account_info_;
};

TEST_F(SyncExplicitPassphraseClientAshTest, ShouldGetDecryptionKey) {
  ON_CALL(*sync_user_settings(), GetExplicitPassphraseDecryptionNigoriKey())
      .WillByDefault(MakeTestNigoriKey);

  crosapi::mojom::NigoriKeyPtr nigori_key =
      GetDecryptionNigoriKey(GetSyncingAccountKey());
  ASSERT_FALSE(nigori_key.is_null());

  crosapi::mojom::NigoriKeyPtr expected_nigori_key = MakeTestMojoNigoriKey();
  EXPECT_THAT(nigori_key->encryption_key,
              Eq(expected_nigori_key->encryption_key));
  EXPECT_THAT(nigori_key->mac_key, Eq(expected_nigori_key->mac_key));
}

TEST_F(SyncExplicitPassphraseClientAshTest,
       ShouldValidateAccountWhenGettingDecryptionKey) {
  crosapi::mojom::AccountKeyPtr wrong_account_key =
      crosapi::mojom::AccountKey::New();
  wrong_account_key->id = "user2";
  wrong_account_key->account_type = crosapi::mojom::AccountType::kGaia;

  crosapi::mojom::NigoriKeyPtr nigori_key =
      GetDecryptionNigoriKey(std::move(wrong_account_key));
  EXPECT_TRUE(nigori_key.is_null());
}

TEST_F(SyncExplicitPassphraseClientAshTest,
       ShouldHandleAbsenseOfKeyWhenGettingDecryptionKey) {
  crosapi::mojom::NigoriKeyPtr nigori_key =
      GetDecryptionNigoriKey(GetSyncingAccountKey());
  EXPECT_TRUE(nigori_key.is_null());
}

TEST_F(SyncExplicitPassphraseClientAshTest, ShouldSetDecryptionKey) {
  EXPECT_CALL(*sync_user_settings(),
              SetExplicitPassphraseDecryptionNigoriKey(NotNull()));
  client()->SetDecryptionNigoriKey(GetSyncingAccountKey(),
                                   MakeTestMojoNigoriKey());
}

TEST_F(SyncExplicitPassphraseClientAshTest,
       ShouldValidateAccountWhenSettingDecryptionKey) {
  crosapi::mojom::AccountKeyPtr wrong_account_key =
      crosapi::mojom::AccountKey::New();
  wrong_account_key->id = "user2";
  wrong_account_key->account_type = crosapi::mojom::AccountType::kGaia;

  EXPECT_CALL(*sync_user_settings(), SetExplicitPassphraseDecryptionNigoriKey)
      .Times(0);
  client()->SetDecryptionNigoriKey(std::move(wrong_account_key),
                                   MakeTestMojoNigoriKey());
}

TEST_F(SyncExplicitPassphraseClientAshTest,
       ShouldHandleNullKeyWhenSettingDecryptionKey) {
  EXPECT_CALL(*sync_user_settings(), SetExplicitPassphraseDecryptionNigoriKey)
      .Times(0);
  client()->SetDecryptionNigoriKey(GetSyncingAccountKey(), nullptr);
}

TEST_F(SyncExplicitPassphraseClientAshTest,
       ShouldHandleInvalidKeyWhenSettingDecryptionKey) {
  EXPECT_CALL(*sync_user_settings(), SetExplicitPassphraseDecryptionNigoriKey)
      .Times(0);

  crosapi::mojom::NigoriKeyPtr mojo_nigori_key =
      crosapi::mojom::NigoriKey::New();
  // Nigori deserialization fails with a wrong key length.
  mojo_nigori_key->encryption_key = {1, 2, 3};
  mojo_nigori_key->mac_key = {1, 2, 3};

  EXPECT_CALL(*sync_user_settings(), SetExplicitPassphraseDecryptionNigoriKey)
      .Times(0);
  client()->SetDecryptionNigoriKey(GetSyncingAccountKey(),
                                   std::move(mojo_nigori_key));
}

TEST_F(SyncExplicitPassphraseClientAshTest,
       ShouldNotifyObserverAboutPassphraseRequired) {
  TestSyncExplicitPassphraseClientObserver observer;
  observer.Observe(client());

  ASSERT_THAT(observer.GetNumOnPassphraseAvailableCalls(), Eq(0));
  ASSERT_THAT(observer.GetNumOnPassphraseRequiredCalls(), Eq(0));

  // Mimic entering passphrase required state.
  ON_CALL(*sync_user_settings(), IsPassphraseRequired())
      .WillByDefault(Return(true));
  client()->OnStateChanged(sync_service());
  client()->FlushMojoForTesting();
  EXPECT_THAT(observer.GetNumOnPassphraseAvailableCalls(), Eq(0));
  EXPECT_THAT(observer.GetNumOnPassphraseRequiredCalls(), Eq(1));
}

TEST_F(SyncExplicitPassphraseClientAshTest,
       ShouldNotifyObserverAboutPassphraseAvailable) {
  TestSyncExplicitPassphraseClientObserver observer;
  observer.Observe(client());

  ASSERT_THAT(observer.GetNumOnPassphraseAvailableCalls(), Eq(0));
  ASSERT_THAT(observer.GetNumOnPassphraseRequiredCalls(), Eq(0));

  // Mimic passphrase being entered by the user.
  ON_CALL(*sync_user_settings(), IsUsingExplicitPassphrase())
      .WillByDefault(Return(true));
  ON_CALL(*sync_user_settings(), IsPassphraseRequired())
      .WillByDefault(Return(false));
  client()->OnStateChanged(sync_service());
  client()->FlushMojoForTesting();
  EXPECT_THAT(observer.GetNumOnPassphraseAvailableCalls(), Eq(1));
  EXPECT_THAT(observer.GetNumOnPassphraseRequiredCalls(), Eq(0));
}

TEST_F(SyncExplicitPassphraseClientAshTest,
       ShouldNotifyNewObserverAboutPassphraseRequired) {
  // Mimic entering passphrase required state.
  ON_CALL(*sync_user_settings(), IsPassphraseRequired())
      .WillByDefault(Return(true));
  client()->OnStateChanged(sync_service());
  client()->FlushMojoForTesting();

  // Add new observer and ensure it's notified about passphrase required state.
  TestSyncExplicitPassphraseClientObserver observer;
  observer.Observe(client());
  client()->FlushMojoForTesting();
  EXPECT_THAT(observer.GetNumOnPassphraseAvailableCalls(), Eq(0));
  EXPECT_THAT(observer.GetNumOnPassphraseRequiredCalls(), Eq(1));
}

TEST_F(SyncExplicitPassphraseClientAshTest,
       ShouldNotifyNewObserverAboutPassphraseAvailable) {
  // Mimic passphrase being entered by the user.
  ON_CALL(*sync_user_settings(), IsUsingExplicitPassphrase())
      .WillByDefault(Return(true));
  ON_CALL(*sync_user_settings(), IsPassphraseRequired())
      .WillByDefault(Return(false));
  client()->OnStateChanged(sync_service());
  client()->FlushMojoForTesting();

  // Add new observer and ensure it's notified about passphrase available state.
  TestSyncExplicitPassphraseClientObserver observer;
  observer.Observe(client());
  client()->FlushMojoForTesting();
  EXPECT_THAT(observer.GetNumOnPassphraseAvailableCalls(), Eq(1));
  EXPECT_THAT(observer.GetNumOnPassphraseRequiredCalls(), Eq(0));
}

}  // namespace
}  // namespace ash
