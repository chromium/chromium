// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/sync/sync_explicit_passphrase_client_lacros.h"

#include "base/observer_list.h"
#include "base/test/task_environment.h"
#include "chromeos/crosapi/mojom/account_manager.mojom.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_manager_util.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/chromeos/explicit_passphrase_mojo_utils.h"
#include "components/sync/driver/mock_sync_service.h"
#include "components/sync/driver/sync_service_observer.h"
#include "components/sync/driver/sync_user_settings_mock.h"
#include "components/sync/engine/nigori/key_derivation_params.h"
#include "components/sync/engine/nigori/nigori.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::_;
using testing::ByMove;
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

crosapi::mojom::AccountKeyPtr MakeMojoAccountKey(
    const CoreAccountInfo& account_info) {
  return account_manager::ToMojoAccountKey(account_manager::AccountKey(
      account_info.gaia, account_manager::AccountType::kGaia));
}

class FakeSyncExplicitPassphraseClientAsh
    : public crosapi::mojom::SyncExplicitPassphraseClient {
 public:
  FakeSyncExplicitPassphraseClientAsh(
      crosapi::mojom::AccountKeyPtr expected_account_key)
      : expected_account_key_(std::move(expected_account_key)) {}

  FakeSyncExplicitPassphraseClientAsh(
      const FakeSyncExplicitPassphraseClientAsh& other) = delete;
  FakeSyncExplicitPassphraseClientAsh& operator=(
      const FakeSyncExplicitPassphraseClientAsh& other) = delete;
  ~FakeSyncExplicitPassphraseClientAsh() override = default;

  void BindReceiver(
      mojo::PendingReceiver<crosapi::mojom::SyncExplicitPassphraseClient>
          pending_receiver) {
    receiver_.Bind(std::move(pending_receiver));
  }

  void SetNigoriKeyAvailable(bool nigori_key_available) {
    nigori_key_available_ = nigori_key_available;
  }

  const crosapi::mojom::NigoriKeyPtr& last_passed_nigori_key() {
    return last_passed_nigori_key_;
  }

  bool get_nigori_key_called() { return get_nigori_key_called_; }

  bool set_nigori_key_called() { return set_nigori_key_called_; }

  void NotifyObserversPassphraseRequired() {
    for (auto& observer : observers_) {
      observer->OnPassphraseRequired();
    }
    observers_.FlushForTesting();
  }

  void NotifyObserversPassphraseAvailable() {
    for (auto& observer : observers_) {
      observer->OnPassphraseAvailable();
    }
    observers_.FlushForTesting();
  }

  // crosapi::mojom::SyncExplicitPassphraseClient implementation.
  void AddObserver(
      mojo::PendingRemote<crosapi::mojom::SyncExplicitPassphraseClientObserver>
          observer) override {
    observers_.Add(std::move(observer));
  }

  void GetDecryptionNigoriKey(
      crosapi::mojom::AccountKeyPtr mojo_account_key,
      GetDecryptionNigoriKeyCallback callback) override {
    get_nigori_key_called_ = true;
    EXPECT_TRUE(ValidateAccountKey(mojo_account_key));
    if (nigori_key_available_) {
      std::move(callback).Run(MakeTestMojoNigoriKey());
    } else {
      std::move(callback).Run(nullptr);
    }
  }

  void SetDecryptionNigoriKey(
      crosapi::mojom::AccountKeyPtr mojo_account_key,
      crosapi::mojom::NigoriKeyPtr mojo_nigori_key) override {
    set_nigori_key_called_ = true;
    EXPECT_TRUE(ValidateAccountKey(mojo_account_key));
    last_passed_nigori_key_ = std::move(mojo_nigori_key);
  }

 private:
  bool ValidateAccountKey(
      const crosapi::mojom::AccountKeyPtr& passed_account_key) {
    return expected_account_key_.Equals(passed_account_key);
  }

  mojo::RemoteSet<crosapi::mojom::SyncExplicitPassphraseClientObserver>
      observers_;
  mojo::Receiver<crosapi::mojom::SyncExplicitPassphraseClient> receiver_{this};
  crosapi::mojom::AccountKeyPtr expected_account_key_;
  bool nigori_key_available_ = false;
  bool get_nigori_key_called_ = false;
  bool set_nigori_key_called_ = false;
  crosapi::mojom::NigoriKeyPtr last_passed_nigori_key_;
};

class FakeSyncMojoService : public crosapi::mojom::SyncService {
 public:
  FakeSyncMojoService(
      crosapi::mojom::AccountKeyPtr expected_account_key,
      mojo::PendingReceiver<crosapi::mojom::SyncService> pending_receiver)
      : client_ash_(std::move(expected_account_key)),
        receiver_(this, std::move(pending_receiver)) {}

  FakeSyncMojoService(const FakeSyncMojoService& other) = delete;
  FakeSyncMojoService& operator=(const FakeSyncMojoService& other) = delete;
  ~FakeSyncMojoService() override = default;

  // crosapi::mojom::SyncService implementation.
  void BindExplicitPassphraseClient(
      mojo::PendingReceiver<crosapi::mojom::SyncExplicitPassphraseClient>
          receiver) override {
    client_ash_.BindReceiver(std::move(receiver));
  }

  FakeSyncExplicitPassphraseClientAsh* client_ash() { return &client_ash_; }

 private:
  FakeSyncExplicitPassphraseClientAsh client_ash_;
  mojo::Receiver<crosapi::mojom::SyncService> receiver_;
};

class SyncExplicitPassphraseClientLacrosTest : public testing::Test {
 public:
  SyncExplicitPassphraseClientLacrosTest() {
    sync_account_info_.gaia = "user";

    sync_mojo_service_ = std::make_unique<FakeSyncMojoService>(
        MakeMojoAccountKey(sync_account_info_),
        sync_mojo_service_remote_.BindNewPipeAndPassReceiver());
  }

  void SetUp() override {
    ON_CALL(sync_service_, GetAccountInfo())
        .WillByDefault(Return(sync_account_info_));
    ON_CALL(sync_service_, AddObserver(_))
        .WillByDefault([this](syncer::SyncServiceObserver* observer) {
          sync_service_observers_.AddObserver(observer);
        });
    ON_CALL(sync_service_, RemoveObserver(_))
        .WillByDefault([this](syncer::SyncServiceObserver* observer) {
          sync_service_observers_.RemoveObserver(observer);
        });

    client_lacros_ = std::make_unique<SyncExplicitPassphraseClientLacros>(
        &sync_service_, &sync_mojo_service_remote_);
    // Needed to trigger AddObserver() call.
    sync_mojo_service_remote_.FlushForTesting();
  }

  void MimicLacrosPassphraseRequired() {
    ON_CALL(*sync_service_.GetMockUserSettings(), IsPassphraseRequired())
        .WillByDefault(Return(true));
    ON_CALL(*sync_service_.GetMockUserSettings(), IsUsingExplicitPassphrase())
        .WillByDefault(Return(true));
    ON_CALL(*sync_service_.GetMockUserSettings(), GetDecryptionNigoriKey())
        .WillByDefault(Return(ByMove(nullptr)));
    for (auto& observer : sync_service_observers_) {
      observer.OnStateChanged(&sync_service_);
    }
  }

  void MimicLacrosPassphraseAvailable() {
    ON_CALL(*sync_service_.GetMockUserSettings(), IsPassphraseRequired())
        .WillByDefault(Return(false));
    ON_CALL(*sync_service_.GetMockUserSettings(), IsUsingExplicitPassphrase())
        .WillByDefault(Return(true));
    ON_CALL(*sync_service_.GetMockUserSettings(), GetDecryptionNigoriKey())
        .WillByDefault(MakeTestNigoriKey);
    for (auto& observer : sync_service_observers_) {
      observer.OnStateChanged(&sync_service_);
    }
  }

  SyncExplicitPassphraseClientLacros* client_lacros() {
    return client_lacros_.get();
  }

  FakeSyncExplicitPassphraseClientAsh* client_ash() {
    return sync_mojo_service_->client_ash();
  }

  syncer::SyncUserSettingsMock* user_settings() {
    return sync_service_.GetMockUserSettings();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  testing::NiceMock<syncer::MockSyncService> sync_service_;
  base::ObserverList<syncer::SyncServiceObserver,
                     /*check_empty=*/true>::Unchecked sync_service_observers_;

  CoreAccountInfo sync_account_info_;
  mojo::Remote<crosapi::mojom::SyncService> sync_mojo_service_remote_;
  std::unique_ptr<FakeSyncMojoService> sync_mojo_service_;

  std::unique_ptr<SyncExplicitPassphraseClientLacros> client_lacros_;
};

TEST_F(SyncExplicitPassphraseClientLacrosTest, ShouldPassNigoriKeyToAsh) {
  MimicLacrosPassphraseAvailable();
  client_ash()->NotifyObserversPassphraseRequired();
  client_lacros()->FlushMojoForTesting();
  EXPECT_TRUE(
      client_ash()->last_passed_nigori_key().Equals(MakeTestMojoNigoriKey()));
}

TEST_F(SyncExplicitPassphraseClientLacrosTest, ShouldGetNigoriKeyFromAsh) {
  MimicLacrosPassphraseRequired();

  EXPECT_CALL(*user_settings(), SetDecryptionNigoriKey(NotNull()));
  client_ash()->SetNigoriKeyAvailable(true);
  client_ash()->NotifyObserversPassphraseAvailable();
}

TEST_F(SyncExplicitPassphraseClientLacrosTest,
       ShouldHandleFailedGetNigoriKeyFromAsh) {
  MimicLacrosPassphraseRequired();
  // client_ash()->SetNigoriKeyAvailable(true) not called,
  // GetDecryptionNigoriKey() IPC will return nullptr.
  EXPECT_CALL(*user_settings(), SetDecryptionNigoriKey(_)).Times(0);
  client_ash()->NotifyObserversPassphraseAvailable();
}

TEST_F(SyncExplicitPassphraseClientLacrosTest,
       ShouldHandleFailedGetDecryptionNigoriKeyFromLacros) {
  MimicLacrosPassphraseAvailable();
  // Mimic rare corner case, when IsPassphraseAvailable() false positive
  // detection happens.
  ON_CALL(*user_settings(), GetDecryptionNigoriKey())
      .WillByDefault(Return(ByMove(nullptr)));
  client_ash()->NotifyObserversPassphraseRequired();
  client_lacros()->FlushMojoForTesting();
  EXPECT_FALSE(client_ash()->set_nigori_key_called());
}

TEST_F(SyncExplicitPassphraseClientLacrosTest, ShouldNotPassNigoriKeyToAsh) {
  MimicLacrosPassphraseAvailable();
  // client_ash() doesn't notify about passphrase required, client_lacros()
  // shouldn't issue redundant IPC.
  client_lacros()->FlushMojoForTesting();
  EXPECT_FALSE(client_ash()->set_nigori_key_called());
}

TEST_F(SyncExplicitPassphraseClientLacrosTest, ShouldNotGetNigoriKeyFromAsh) {
  MimicLacrosPassphraseRequired();
  // client_ash() doesn't notify about passphrase available, client_lacros()
  // shouldn't issue redundant IPC.
  client_lacros()->FlushMojoForTesting();
  EXPECT_FALSE(client_ash()->get_nigori_key_called());
}

}  // namespace
