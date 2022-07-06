// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "chrome/browser/sync/test/integration/encryption_helper.h"
#include "chrome/browser/sync/test/integration/passwords_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/common/chrome_constants.h"
#include "chromeos/crosapi/mojom/account_manager.mojom.h"
#include "chromeos/crosapi/mojom/sync.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/sync/chromeos/explicit_passphrase_mojo_utils.h"
#include "components/sync/engine/nigori/nigori.h"
#include "components/sync/nigori/nigori_test_utils.h"
#include "components/sync/test/fake_server/fake_server_nigori_helper.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::_;

std::string ComputeKeyName(const syncer::Nigori& nigori) {
  std::string key_name;
  nigori.Permute(syncer::Nigori::Password, syncer::kNigoriKeyName, &key_name);
  return key_name;
}

MATCHER_P(AccountKeyEq, expected_account_key, "") {
  const crosapi::mojom::AccountKeyPtr& given_account_key = arg;
  return given_account_key->id == expected_account_key.id &&
         given_account_key->account_type == expected_account_key.account_type;
}

MATCHER_P(MojoNigoriCanDecryptServerNigori, fake_server, "") {
  const crosapi::mojom::NigoriKeyPtr& mojo_nigori = arg;
  sync_pb::NigoriSpecifics server_specifics;
  fake_server::GetServerNigori(fake_server, &server_specifics);
  return mojo_nigori && ComputeKeyName(*syncer::NigoriFromMojo(*mojo_nigori)) ==
                            server_specifics.encryption_keybag().key_name();
}

class MockSyncExplicitPassphraseClientAsh
    : public crosapi::mojom::SyncExplicitPassphraseClient {
 public:
  MockSyncExplicitPassphraseClientAsh() = default;
  ~MockSyncExplicitPassphraseClientAsh() override = default;

  MOCK_METHOD(void,
              AddObserver,
              (mojo::PendingRemote<
                  crosapi::mojom::SyncExplicitPassphraseClientObserver>),
              (override));
  MOCK_METHOD(void,
              GetDecryptionNigoriKey,
              (crosapi::mojom::AccountKeyPtr, GetDecryptionNigoriKeyCallback),
              (override));
  MOCK_METHOD(void,
              SetDecryptionNigoriKey,
              (crosapi::mojom::AccountKeyPtr, crosapi::mojom::NigoriKeyPtr),
              (override));
};

class MockSyncMojoService : public crosapi::mojom::SyncService {
 public:
  MockSyncMojoService() = default;
  ~MockSyncMojoService() override = default;

  MOCK_METHOD(
      void,
      BindExplicitPassphraseClient,
      (mojo::PendingReceiver<crosapi::mojom::SyncExplicitPassphraseClient>),
      (override));
  MOCK_METHOD(void,
              BindUserSettingsClient,
              (mojo::PendingReceiver<crosapi::mojom::SyncUserSettingsClient>),
              (override));
};

class SyncCustomPassphraseSharingLacrosBrowserTest : public SyncTest {
 public:
  SyncCustomPassphraseSharingLacrosBrowserTest() : SyncTest(SINGLE_CLIENT) {}
  SyncCustomPassphraseSharingLacrosBrowserTest(
      const SyncCustomPassphraseSharingLacrosBrowserTest&) = delete;
  SyncCustomPassphraseSharingLacrosBrowserTest& operator=(
      const SyncCustomPassphraseSharingLacrosBrowserTest&) = delete;
  ~SyncCustomPassphraseSharingLacrosBrowserTest() override = default;

  base::FilePath GetProfileBaseName(int index) override {
    // Custom passphrase sharing is enabled only for the main profile, so
    // SyncTest should setup sync using it.
    DCHECK_EQ(index, 0);
    return base::FilePath(chrome::kInitialProfile);
  }

  // This test replaces production SyncService Crosapi interface with a mock.
  // It needs to be done before connection between Ash and Lacros explicit
  // passphrase clients is established (during creation of browser extra parts),
  // but after LacrosService is initialized. Thus CreatedBrowserMainParts() is
  // the only available option.
  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    SyncTest::CreatedBrowserMainParts(browser_main_parts);

    // If SyncService Crosapi interface is not available on this version of
    // ash-chrome, this test suite will no-op.
    if (!IsServiceAvailable()) {
      return;
    }

    // Replace the production SyncService Crosapi interface with a mock for
    // testing.
    mojo::Remote<crosapi::mojom::SyncService>& remote =
        chromeos::LacrosService::Get()
            ->GetRemote<crosapi::mojom::SyncService>();
    remote.reset();
    sync_mojo_service_receiver_.Bind(remote.BindNewPipeAndPassReceiver());

    // Lacros client is not expected to call these methods more than once.
    ON_CALL(sync_mojo_service_, BindExplicitPassphraseClient)
        .WillByDefault(testing::Invoke(
            this, &SyncCustomPassphraseSharingLacrosBrowserTest::
                      BindExplicitPassphraseClient));

    ON_CALL(client_ash_, AddObserver)
        .WillByDefault(testing::Invoke(
            this,
            &SyncCustomPassphraseSharingLacrosBrowserTest::AddClientObserver));
  }

  bool IsServiceAvailable() const {
    const chromeos::LacrosService* lacros_service =
        chromeos::LacrosService::Get();
    return lacros_service &&
           lacros_service->IsAvailable<crosapi::mojom::SyncService>();
  }

  crosapi::mojom::AccountKey GetSyncingUserAccountKey() {
    crosapi::mojom::AccountKey account_key;
    account_key.id = GetSyncService(0)->GetAccountInfo().gaia;
    account_key.account_type = crosapi::mojom::AccountType::kGaia;
    return account_key;
  }

  MockSyncExplicitPassphraseClientAsh* client_ash() { return &client_ash_; }

  crosapi::mojom::SyncExplicitPassphraseClientObserver* client_observer() {
    return client_observer_remote_.get();
  }

 private:
  void BindExplicitPassphraseClient(
      mojo::PendingReceiver<crosapi::mojom::SyncExplicitPassphraseClient>
          pending_receiver) {
    client_ash_receiver_.Bind(std::move(pending_receiver));
  }

  void AddClientObserver(
      mojo::PendingRemote<crosapi::mojom::SyncExplicitPassphraseClientObserver>
          pending_remote) {
    client_observer_remote_.Bind(std::move(pending_remote));
  }

  testing::NiceMock<MockSyncMojoService> sync_mojo_service_;
  testing::NiceMock<MockSyncExplicitPassphraseClientAsh> client_ash_;

  // Mojo fields order is important to allow safe use of `this` when passing
  // callbacks.
  mojo::Remote<crosapi::mojom::SyncExplicitPassphraseClientObserver>
      client_observer_remote_;
  mojo::Receiver<crosapi::mojom::SyncExplicitPassphraseClient>
      client_ash_receiver_{&client_ash_};
  mojo::Receiver<crosapi::mojom::SyncService> sync_mojo_service_receiver_{
      &sync_mojo_service_};
};

IN_PROC_BROWSER_TEST_F(SyncCustomPassphraseSharingLacrosBrowserTest,
                       ShouldGetDecryptionKeyFromAsh) {
  if (!IsServiceAvailable()) {
    GTEST_SKIP() << "Unsupported Ash version.";
  }

  ASSERT_TRUE(SetupSync());

  // Mimic custom passphrase being set by other client.
  const syncer::KeyParamsForTesting kKeyParams =
      syncer::ScryptPassphraseKeyParamsForTesting("hunter2");
  fake_server::SetNigoriInFakeServer(
      syncer::BuildCustomPassphraseNigoriSpecifics(kKeyParams),
      GetFakeServer());

  // Inject server password encrypted with a custom passphrase.
  const password_manager::PasswordForm password_form =
      passwords_helper::CreateTestPasswordForm(0);
  passwords_helper::InjectEncryptedServerPassword(
      password_form, kKeyParams.password, kKeyParams.derivation_params,
      GetFakeServer());

  // Data isn't decryptable yet, client should enter passphrase required state.
  ASSERT_TRUE(PassphraseRequiredChecker(GetSyncService(0)).Wait());

  // Mimic passphrase being provided by Ash, verify that passphrase is no longer
  // required and the data is decryptable.
  EXPECT_CALL(*client_ash(), GetDecryptionNigoriKey(
                                 AccountKeyEq(GetSyncingUserAccountKey()), _))
      .WillOnce([&kKeyParams](auto account_key, auto callback) {
        std::move(callback).Run(
            syncer::NigoriToMojo(*syncer::Nigori::CreateByDerivation(
                kKeyParams.derivation_params, kKeyParams.password)));
      });
  client_observer()->OnPassphraseAvailable();
  EXPECT_TRUE(PassphraseAcceptedChecker(GetSyncService(0)).Wait());
  EXPECT_TRUE(PasswordFormsChecker(0, {password_form}).Wait());
}

IN_PROC_BROWSER_TEST_F(SyncCustomPassphraseSharingLacrosBrowserTest,
                       ShouldExposeEncryptionKeyWhenSetDecryptionPassphrase) {
  if (!IsServiceAvailable()) {
    GTEST_SKIP() << "Unsupported Ash version.";
  }

  ASSERT_TRUE(SetupSync());

  // Mimic custom passphrase being set by other client.
  const syncer::KeyParamsForTesting kKeyParams =
      syncer::ScryptPassphraseKeyParamsForTesting("hunter2");
  fake_server::SetNigoriInFakeServer(
      syncer::BuildCustomPassphraseNigoriSpecifics(kKeyParams),
      GetFakeServer());

  // Mimic Ash received the remote update and indicates that passphrase is
  // required.
  client_observer()->OnPassphraseRequired();

  ASSERT_TRUE(PassphraseRequiredChecker(GetSyncService(0)).Wait());

  // Mimic that user enters the passphrase, key should be exposed to Ash.
  base::RunLoop run_loop;
  EXPECT_CALL(
      *client_ash(),
      SetDecryptionNigoriKey(AccountKeyEq(GetSyncingUserAccountKey()),
                             MojoNigoriCanDecryptServerNigori(GetFakeServer())))
      .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));

  ASSERT_TRUE(GetSyncService(0)->GetUserSettings()->SetDecryptionPassphrase(
      kKeyParams.password));
  ASSERT_TRUE(PassphraseAcceptedChecker(GetSyncService(0)).Wait());

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(SyncCustomPassphraseSharingLacrosBrowserTest,
                       ShouldExposeEncryptionKeyWhenSetEncryptionPassphrase) {
  if (!IsServiceAvailable()) {
    GTEST_SKIP() << "Unsupported Ash version.";
  }

  ASSERT_TRUE(SetupSync());

  const std::string kPassphrase = "hunter2";
  GetSyncService(0)->GetUserSettings()->SetEncryptionPassphrase(kPassphrase);
  ASSERT_TRUE(
      ServerPassphraseTypeChecker(syncer::PassphraseType::kCustomPassphrase)
          .Wait());

  // Mimic Ash received the remote update and indicates that passphrase is
  // required, key should be exposed to Ash.
  base::RunLoop run_loop;
  EXPECT_CALL(
      *client_ash(),
      SetDecryptionNigoriKey(AccountKeyEq(GetSyncingUserAccountKey()),
                             MojoNigoriCanDecryptServerNigori(GetFakeServer())))
      .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));

  client_observer()->OnPassphraseRequired();
  run_loop.Run();
}

}  // namespace
