// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/sync/sync_explicit_passphrase_client_lacros.h"

#include <utility>

#include "base/observer_list.h"
#include "base/test/task_environment.h"
#include "chromeos/crosapi/mojom/account_manager.mojom.h"
#include "chromeos/crosapi/mojom/sync.mojom.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_manager_util.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/chromeos/explicit_passphrase_mojo_utils.h"
#include "components/sync/engine/nigori/key_derivation_params.h"
#include "components/sync/engine/nigori/nigori.h"
#include "components/sync/service/sync_service_observer.h"
#include "components/sync/test/fake_sync_explicit_passphrase_client_ash.h"
#include "components/sync/test/fake_sync_mojo_service.h"
#include "components/sync/test/mock_sync_service.h"
#include "components/sync/test/sync_user_settings_mock.h"
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

class SyncExplicitPassphraseClientLacrosTest : public testing::Test {
 public:
  SyncExplicitPassphraseClientLacrosTest() { sync_account_info_.gaia = "user"; }

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

    mojo::Remote<crosapi::mojom::SyncExplicitPassphraseClient> client_remote;
    client_ash_.BindReceiver(client_remote.BindNewPipeAndPassReceiver());

    client_ash_.SetExpectedAccountKey(MakeMojoAccountKey(sync_account_info_));

    client_lacros_ = std::make_unique<SyncExplicitPassphraseClientLacros>(
        std::move(client_remote), &sync_service_);
    // Needed to trigger AddObserver() call.
    client_lacros_->FlushMojoForTesting();
  }

  void MimicLacrosPassphraseRequired() {
    ON_CALL(*sync_service_.GetMockUserSettings(), IsPassphraseRequired())
        .WillByDefault(Return(true));
    ON_CALL(*sync_service_.GetMockUserSettings(), IsUsingExplicitPassphrase())
        .WillByDefault(Return(true));
    ON_CALL(*sync_service_.GetMockUserSettings(),
            GetExplicitPassphraseDecryptionNigoriKey())
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
    ON_CALL(*sync_service_.GetMockUserSettings(),
            GetExplicitPassphraseDecryptionNigoriKey())
        .WillByDefault(MakeTestNigoriKey);
    for (auto& observer : sync_service_observers_) {
      observer.OnStateChanged(&sync_service_);
    }
  }

  SyncExplicitPassphraseClientLacros& client_lacros() {
    return *client_lacros_;
  }

  syncer::FakeSyncExplicitPassphraseClientAsh& client_ash() {
    return client_ash_;
  }

  syncer::SyncUserSettingsMock& user_settings() {
    return *sync_service_.GetMockUserSettings();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  testing::NiceMock<syncer::MockSyncService> sync_service_;
  base::ObserverList<syncer::SyncServiceObserver,
                     /*check_empty=*/true>::Unchecked sync_service_observers_;

  CoreAccountInfo sync_account_info_;
  syncer::FakeSyncExplicitPassphraseClientAsh client_ash_;

  std::unique_ptr<SyncExplicitPassphraseClientLacros> client_lacros_;
};

TEST_F(SyncExplicitPassphraseClientLacrosTest,
       ShouldPassNigoriKeyToAshWhenPassphraseAlreadyAvailable) {
  // Corresponds to scenario, when custom passphrase was enabled in Lacros,
  // Lacros will have passphrase available almost immediately, while Ash will
  // require it only after Sync cycle completion. Lacros should pass passphrase
  // to Ash once it becomes required by Ash.
  MimicLacrosPassphraseAvailable();
  client_ash().MimicPassphraseRequired(MakeTestMojoNigoriKey());
  client_lacros().FlushMojoForTesting();

  EXPECT_TRUE(client_ash().IsSetDecryptionNigoriKeyCalled());
  EXPECT_FALSE(client_ash().IsPassphraseRequired());
}

TEST_F(SyncExplicitPassphraseClientLacrosTest,
       ShouldPassNigoriKeyToAshWhenPassphraseAvailableAfterRequiredByAsh) {
  // Corresponds to scenario, when custom passphrase was enabled on other
  // device, passphrase will become required in both Ash and Lacros roughly at
  // the same time. Once user enters the decryption passphrase in Lacros, it
  // should be passed to Ash.
  client_ash().MimicPassphraseRequired(MakeTestMojoNigoriKey());
  MimicLacrosPassphraseAvailable();
  client_lacros().FlushMojoForTesting();

  EXPECT_TRUE(client_ash().IsSetDecryptionNigoriKeyCalled());
  EXPECT_FALSE(client_ash().IsPassphraseRequired());
}

TEST_F(SyncExplicitPassphraseClientLacrosTest, ShouldGetNigoriKeyFromAsh) {
  MimicLacrosPassphraseRequired();

  EXPECT_CALL(user_settings(),
              SetExplicitPassphraseDecryptionNigoriKey(NotNull()));
  client_ash().MimicPassphraseAvailable(MakeTestMojoNigoriKey());
}

TEST_F(SyncExplicitPassphraseClientLacrosTest,
       ShouldHandleFailedGetNigoriKeyFromAsh) {
  MimicLacrosPassphraseRequired();
  // client_ash() will notify observers that passphrase is available, but expose
  // nullptr when GetDecryptionNigoriKey() is called. Lacros client should
  // handle this nullptr and shouldn't call
  // SyncUserSettings::SetExplicitPassphraseDecryptionNigoriKey().
  EXPECT_CALL(user_settings(), SetExplicitPassphraseDecryptionNigoriKey(_))
      .Times(0);
  client_ash().MimicPassphraseAvailable(/*nigori_key=*/nullptr);
}

TEST_F(SyncExplicitPassphraseClientLacrosTest,
       ShouldHandleFailedGetDecryptionNigoriKeyFromLacros) {
  MimicLacrosPassphraseAvailable();
  // Mimic rare corner case, when IsPassphraseAvailable() false positive
  // detection happens.
  ON_CALL(user_settings(), GetExplicitPassphraseDecryptionNigoriKey())
      .WillByDefault(Return(ByMove(nullptr)));
  client_ash().MimicPassphraseRequired(MakeTestMojoNigoriKey());
  client_lacros().FlushMojoForTesting();

  EXPECT_FALSE(client_ash().IsSetDecryptionNigoriKeyCalled());
}

TEST_F(SyncExplicitPassphraseClientLacrosTest, ShouldNotPassNigoriKeyToAsh) {
  MimicLacrosPassphraseAvailable();
  // client_ash() doesn't notify about passphrase required, client_lacros()
  // shouldn't issue redundant IPC.
  client_lacros().FlushMojoForTesting();
  EXPECT_FALSE(client_ash().IsSetDecryptionNigoriKeyCalled());
}

TEST_F(SyncExplicitPassphraseClientLacrosTest, ShouldNotGetNigoriKeyFromAsh) {
  MimicLacrosPassphraseRequired();
  // client_ash() doesn't notify about passphrase available, client_lacros()
  // shouldn't issue redundant IPC.
  client_lacros().FlushMojoForTesting();
  EXPECT_FALSE(client_ash().IsGetDecryptionNigoriKeyCalled());
}

}  // namespace
