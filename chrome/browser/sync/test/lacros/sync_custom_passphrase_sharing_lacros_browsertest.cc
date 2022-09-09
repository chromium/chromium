// Copyright 2022 The Chromium Authors
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
#include "components/sync/test/fake_server_nigori_helper.h"
#include "components/sync/test/fake_sync_explicit_passphrase_client_ash.h"
#include "components/sync/test/fake_sync_mojo_service.h"
#include "components/sync/test/nigori_test_utils.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

crosapi::mojom::NigoriKeyPtr MakeMojoNigoriKey(
    const syncer::KeyParamsForTesting& key_params) {
  return syncer::NigoriToMojo(*syncer::Nigori::CreateByDerivation(
      key_params.derivation_params, key_params.password));
}

syncer::KeyParamsForTesting
MakeCustomPassphraseKeyParamsFromServerNigoriAndPassphrase(
    const std::string& passphrase,
    fake_server::FakeServer* fake_server) {
  sync_pb::NigoriSpecifics nigori_specifics;
  fake_server::GetServerNigori(fake_server, &nigori_specifics);
  return {syncer::InitCustomPassphraseKeyDerivationParamsFromNigori(
              nigori_specifics),
          passphrase};
}

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

  // This test replaces production SyncService Crosapi interface with a fake.
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

    // Replace the production SyncService Crosapi interface with a fake for
    // testing.
    mojo::Remote<crosapi::mojom::SyncService>& remote =
        chromeos::LacrosService::Get()
            ->GetRemote<crosapi::mojom::SyncService>();
    remote.reset();
    sync_mojo_service_.BindReceiver(remote.BindNewPipeAndPassReceiver());
  }

  bool IsServiceAvailable() const {
    const chromeos::LacrosService* lacros_service =
        chromeos::LacrosService::Get();
    return lacros_service &&
           lacros_service->IsAvailable<crosapi::mojom::SyncService>();
  }

  bool SetupSyncAndSetAccountKeyExpectations() {
    if (!SetupSync()) {
      return false;
    }

    crosapi::mojom::AccountKeyPtr account_key =
        crosapi::mojom::AccountKey::New();
    account_key->id = GetSyncService(0)->GetAccountInfo().gaia;
    account_key->account_type = crosapi::mojom::AccountType::kGaia;
    client_ash().SetExpectedAccountKey(std::move(account_key));

    return true;
  }

  syncer::FakeSyncExplicitPassphraseClientAsh& client_ash() {
    return sync_mojo_service_.GetFakeSyncExplicitPassphraseClientAsh();
  }

 private:
  // Mojo fields order is important to allow safe use of `this` when passing
  // callbacks.
  syncer::FakeSyncMojoService sync_mojo_service_;
};

IN_PROC_BROWSER_TEST_F(SyncCustomPassphraseSharingLacrosBrowserTest,
                       ShouldGetDecryptionKeyFromAsh) {
  if (!IsServiceAvailable()) {
    GTEST_SKIP() << "Unsupported Ash version.";
  }

  ASSERT_TRUE(SetupSyncAndSetAccountKeyExpectations());

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
  client_ash().MimicPassphraseAvailable(MakeMojoNigoriKey(kKeyParams));
  EXPECT_TRUE(PassphraseAcceptedChecker(GetSyncService(0)).Wait());
  EXPECT_TRUE(PasswordFormsChecker(0, {password_form}).Wait());
}

IN_PROC_BROWSER_TEST_F(SyncCustomPassphraseSharingLacrosBrowserTest,
                       ShouldExposeEncryptionKeyWhenSetDecryptionPassphrase) {
  if (!IsServiceAvailable()) {
    GTEST_SKIP() << "Unsupported Ash version.";
  }

  ASSERT_TRUE(SetupSyncAndSetAccountKeyExpectations());

  // Mimic custom passphrase being set by other client.
  const syncer::KeyParamsForTesting kKeyParams =
      syncer::ScryptPassphraseKeyParamsForTesting("hunter2");
  fake_server::SetNigoriInFakeServer(
      syncer::BuildCustomPassphraseNigoriSpecifics(kKeyParams),
      GetFakeServer());

  // Mimic Ash received the remote update and indicates that passphrase is
  // required.
  base::RunLoop run_loop;
  client_ash().MimicPassphraseRequired(
      /*expected_nigori_key=*/MakeMojoNigoriKey(kKeyParams),
      /*passphrase_provided_callback=*/run_loop.QuitClosure());

  ASSERT_TRUE(PassphraseRequiredChecker(GetSyncService(0)).Wait());

  // Mimic that user enters the passphrase, key should be exposed to Ash.
  ASSERT_TRUE(GetSyncService(0)->GetUserSettings()->SetDecryptionPassphrase(
      kKeyParams.password));
  ASSERT_TRUE(PassphraseAcceptedChecker(GetSyncService(0)).Wait());

  run_loop.Run();
  EXPECT_FALSE(client_ash().IsPassphraseRequired());
}

IN_PROC_BROWSER_TEST_F(SyncCustomPassphraseSharingLacrosBrowserTest,
                       ShouldExposeEncryptionKeyWhenSetEncryptionPassphrase) {
  if (!IsServiceAvailable()) {
    GTEST_SKIP() << "Unsupported Ash version.";
  }

  ASSERT_TRUE(SetupSyncAndSetAccountKeyExpectations());

  const std::string kPassphrase = "hunter2";
  GetSyncService(0)->GetUserSettings()->SetEncryptionPassphrase(kPassphrase);
  ASSERT_TRUE(
      ServerPassphraseTypeChecker(syncer::PassphraseType::kCustomPassphrase)
          .Wait());

  // Mimic Ash received the remote update and indicates that passphrase is
  // required, key should be exposed to Ash.
  base::RunLoop run_loop;
  client_ash().MimicPassphraseRequired(
      /*expected_nigori_key=*/MakeMojoNigoriKey(
          MakeCustomPassphraseKeyParamsFromServerNigoriAndPassphrase(
              kPassphrase, GetFakeServer())),
      /*passphrase_provided_callback=*/run_loop.QuitClosure());

  run_loop.Run();
  EXPECT_FALSE(client_ash().IsPassphraseRequired());
}

}  // namespace
