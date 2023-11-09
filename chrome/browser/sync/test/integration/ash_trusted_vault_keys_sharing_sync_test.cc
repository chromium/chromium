// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/trusted_vault/trusted_vault_service_factory.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/standalone_browser/standalone_browser_features.h"
#include "chromeos/crosapi/mojom/trusted_vault.mojom.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/trusted_vault/features.h"
#include "components/trusted_vault/trusted_vault_client.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "components/trusted_vault/trusted_vault_service.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

using testing::Eq;
using testing::NotNull;

class AshTrustedVaultKeysSharingSyncTest : public SyncTest {
 public:
  AshTrustedVaultKeysSharingSyncTest() : SyncTest(SINGLE_CLIENT) {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {
            ash::standalone_browser::features::kLacrosOnly,
            trusted_vault::kChromeOSTrustedVaultUseWebUIDialog,
            trusted_vault::kChromeOSTrustedVaultClientShared,
        },
        /*disabled_features=*/{});
  }

  ~AshTrustedVaultKeysSharingSyncTest() override = default;

  // SyncTest overrides.
  base::FilePath GetProfileBaseName(int index) override {
    // Need to reuse test user profile for this test - Crosapi explicitly
    // assumes there is only one regular profile.
    CHECK_EQ(index, 0);
    return base::FilePath(
        ash::BrowserContextHelper::kTestUserBrowserContextDirName);
  }

  void SetupCrosapi() {
    ASSERT_TRUE(crosapi::browser_util::IsLacrosEnabled());

    crosapi::CrosapiAsh* crosapi_ash =
        crosapi::CrosapiManager::Get()->crosapi_ash();
    ASSERT_THAT(crosapi_ash, NotNull());

    crosapi_ash->BindTrustedVaultBackend(
        trusted_vault_backend_remote_.BindNewPipeAndPassReceiver());
  }

  trusted_vault::TrustedVaultClient& GetAshSyncTrustedVaultClient() {
    trusted_vault::TrustedVaultService* trusted_vault_service =
        TrustedVaultServiceFactory::GetForProfile(GetProfile(0));
    CHECK(trusted_vault_service);

    trusted_vault::TrustedVaultClient* sync_trusted_vault_client =
        trusted_vault_service->GetTrustedVaultClient(
            trusted_vault::SecurityDomainId::kChromeSync);
    CHECK(sync_trusted_vault_client);
    return *sync_trusted_vault_client;
  }

  std::vector<std::vector<uint8_t>> FetchKeysThroughCrosapi() {
    base::test::TestFuture<std::vector<std::vector<uint8_t>>>
        fetched_keys_future;
    trusted_vault_backend_remote_->FetchKeys(
        GetSyncingUserAccountKey(),
        fetched_keys_future
            .GetCallback<const std::vector<std::vector<uint8_t>>&>());

    return fetched_keys_future.Take();
  }

  CoreAccountInfo GetSyncingUserAccountInfo() {
    return GetSyncService(0)->GetAccountInfo();
  }

  crosapi::mojom::AccountKeyPtr GetSyncingUserAccountKey() {
    auto account_key = crosapi::mojom::AccountKey::New();
    account_key->id = GetSyncService(0)->GetAccountInfo().gaia;
    account_key->account_type = crosapi::mojom::AccountType::kGaia;
    return account_key;
  }

 private:
  base::test::ScopedFeatureList feature_list_;

  mojo::Remote<crosapi::mojom::TrustedVaultBackend>
      trusted_vault_backend_remote_;
};

IN_PROC_BROWSER_TEST_F(AshTrustedVaultKeysSharingSyncTest,
                       ShouldFetchStoredKeysThroughCrosapi) {
  ASSERT_TRUE(SetupSync());

  // Mimic that Ash already has trusted vault key.
  std::vector<std::vector<uint8_t>> trusted_vault_keys = {{1, 2, 3}};
  GetAshSyncTrustedVaultClient().StoreKeys(GetSyncingUserAccountInfo().gaia,
                                           trusted_vault_keys,
                                           /*last_key_version*/ 1);

  // Mimic that Lacros starts and attempts to fetch keys, it should succeed.
  SetupCrosapi();
  EXPECT_THAT(FetchKeysThroughCrosapi(), Eq(trusted_vault_keys));
}

}  // namespace
