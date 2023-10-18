// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sync/test/integration/passwords_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/common/chrome_constants.h"
#include "chromeos/crosapi/mojom/trusted_vault.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/sync/test/fake_server_nigori_helper.h"
#include "components/sync/test/nigori_test_utils.h"
#include "components/trusted_vault/features.h"
#include "components/trusted_vault/test/fake_crosapi_trusted_vault_backend.h"
#include "components/trusted_vault/test/fake_trusted_vault_client.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TrustedVaultKeysSharingLacrosBrowserTest : public SyncTest {
 public:
  TrustedVaultKeysSharingLacrosBrowserTest()
      : SyncTest(SINGLE_CLIENT),
        trusted_vault_client_ash_(/*auto_complete_requests=*/true) {
    override_features_.InitAndEnableFeature(
        trusted_vault::kChromeOSTrustedVaultClientShared);
    fake_crosapi_backend_ =
        std::make_unique<trusted_vault::FakeCrosapiTrustedVaultBackend>(
            &trusted_vault_client_ash_);
  }

  ~TrustedVaultKeysSharingLacrosBrowserTest() override = default;

  base::FilePath GetProfileBaseName(int index) override {
    // TrustedVault keys sharing is enabled only for the main profile, so
    // SyncTest should setup sync using it.
    CHECK_EQ(index, 0);
    return base::FilePath(chrome::kInitialProfile);
  }

  // This test replaces production TrustedVaultBackend Crosapi interface with a
  // fake one. It needs to be done before connection between Ash
  // TrustedVaultBackend and Lacros TrustedVaultClient is established (during
  // creation of Lacros profile), but after LacrosService is initialized. Thus
  // relying on CreatedBrowserMainParts().
  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    SyncTest::CreatedBrowserMainParts(browser_main_parts);

    // Replace the production TrustedVaultBackend Crosapi with a fake for
    // testing.
    chromeos::LacrosService::Get()->InjectRemoteForTesting(
        fake_crosapi_backend_->BindNewPipeAndPassRemote());
  }

  bool SetupSyncAndFakeCrosapiBackend() {
    if (!SetupSync()) {
      return false;
    }

    fake_crosapi_backend_->SetPrimaryAccountInfo(
        GetSyncService(0)->GetAccountInfo());
    return true;
  }

  trusted_vault::FakeTrustedVaultClient& trusted_vault_client_ash() {
    return trusted_vault_client_ash_;
  }

 private:
  base::test::ScopedFeatureList override_features_;

  trusted_vault::FakeTrustedVaultClient trusted_vault_client_ash_;
  std::unique_ptr<trusted_vault::FakeCrosapiTrustedVaultBackend>
      fake_crosapi_backend_;
};

IN_PROC_BROWSER_TEST_F(TrustedVaultKeysSharingLacrosBrowserTest,
                       ShouldFetchKeys) {
  ASSERT_TRUE(SetupSyncAndFakeCrosapiBackend());

  // Mimic that trusted vault key is already available in Ash.
  const std::vector<uint8_t> trusted_vault_key = {1, 2, 3};
  trusted_vault_client_ash().StoreKeys(GetSyncService(0)->GetAccountInfo().gaia,
                                       {trusted_vault_key},
                                       /*last_key_version=*/1);

  // Inject trusted vault Nigori server-side.
  fake_server::SetNigoriInFakeServer(
      syncer::BuildTrustedVaultNigoriSpecifics({trusted_vault_key}),
      GetFakeServer());

  // Inject a password encrypted with trusted vault key server-side.
  const syncer::KeyParamsForTesting trusted_vault_key_params =
      syncer::TrustedVaultKeyParamsForTesting(trusted_vault_key);
  const password_manager::PasswordForm password_form =
      passwords_helper::CreateTestPasswordForm(1);
  passwords_helper::InjectEncryptedServerPassword(
      password_form, trusted_vault_key_params.password,
      trusted_vault_key_params.derivation_params, GetFakeServer());

  // Lacros should be able to fetch keys from Ash and decrypt the passwords.
  EXPECT_TRUE(PasswordFormsChecker(0, {password_form}).Wait());
}

}  // namespace
