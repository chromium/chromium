// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/trusted_vault/crosapi_trusted_vault_client.h"
#include <memory>

#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chromeos/crosapi/mojom/trusted_vault.mojom.h"
#include "components/trusted_vault/test/fake_crosapi_trusted_vault_backend.h"
#include "components/trusted_vault/test/fake_trusted_vault_client.h"
#include "components/trusted_vault/trusted_vault_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::Eq;
using testing::IsEmpty;
using testing::SizeIs;

class MockTrustedVaultObserver
    : public trusted_vault::TrustedVaultClient::Observer {
 public:
  MockTrustedVaultObserver() = default;
  ~MockTrustedVaultObserver() override = default;

  MOCK_METHOD(void, OnTrustedVaultKeysChanged, (), (override));
  MOCK_METHOD(void, OnTrustedVaultRecoverabilityChanged, (), (override));
};

class CrosapiTrustedVaultClientTest : public testing::Test {
 public:
  CrosapiTrustedVaultClientTest() {
    primary_account_info_.gaia = "user";
    crosapi_backend_ =
        std::make_unique<trusted_vault::FakeCrosapiTrustedVaultBackend>(
            &trusted_vault_client_ash_);
    crosapi_backend_->SetPrimaryAccountInfo(primary_account_info_);

    crosapi_backend_->BindReceiver(
        backend_remote_.BindNewPipeAndPassReceiver());

    trusted_vault_client_lacros_ =
        std::make_unique<CrosapiTrustedVaultClient>(&backend_remote_);

    // Needed to complete AddObserver() mojo call.
    crosapi_backend_->FlushMojo();
  }

  ~CrosapiTrustedVaultClientTest() override = default;

  CoreAccountInfo& primary_account_info() { return primary_account_info_; }

  trusted_vault::FakeTrustedVaultClient& trusted_vault_client_ash() {
    return trusted_vault_client_ash_;
  }

  trusted_vault::FakeCrosapiTrustedVaultBackend& crosapi_backend() {
    return *crosapi_backend_;
  }

  CrosapiTrustedVaultClient& trusted_vault_client_lacros() {
    return *trusted_vault_client_lacros_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  CoreAccountInfo primary_account_info_;

  trusted_vault::FakeTrustedVaultClient trusted_vault_client_ash_;
  std::unique_ptr<trusted_vault::FakeCrosapiTrustedVaultBackend>
      crosapi_backend_;
  mojo::Remote<crosapi::mojom::TrustedVaultBackend> backend_remote_;

  std::unique_ptr<CrosapiTrustedVaultClient> trusted_vault_client_lacros_;
};

TEST_F(CrosapiTrustedVaultClientTest, ShouldFetchKeys) {
  const std::vector<std::vector<uint8_t>> keys = {{1, 2, 3}};
  trusted_vault_client_ash().StoreKeys(primary_account_info().gaia, keys,
                                       /*last_key_version=*/1);

  base::MockCallback<
      base::OnceCallback<void(const std::vector<std::vector<uint8_t>>&)>>
      on_keys_fetched;
  EXPECT_CALL(on_keys_fetched, Run(keys));
  trusted_vault_client_lacros().FetchKeys(primary_account_info(),
                                          on_keys_fetched.Get());
  // Fetching keys is quite asynchronous in this setup.
  // 1. Ensure mojo propagates FetchKeys() call to backend.
  crosapi_backend().FlushMojo();
  // 2. Mimics asynchronous fetch completion on trusted_vault_client_ash() side.
  EXPECT_TRUE(trusted_vault_client_ash().CompleteAllPendingRequests());
  // 3. Ensure mojo propagates callback call.
  crosapi_backend().FlushMojo();
}

TEST_F(CrosapiTrustedVaultClientTest, ShouldMarkLocalKeysAsStale) {
  trusted_vault_client_ash().StoreKeys(primary_account_info().gaia,
                                       /*keys=*/{{1, 2, 3}},
                                       /*last_key_version=*/1);

  base::MockCallback<base::OnceCallback<void(bool)>> on_keys_marked_as_stale;
  EXPECT_CALL(on_keys_marked_as_stale, Run(true));
  trusted_vault_client_lacros().MarkLocalKeysAsStale(
      primary_account_info(), on_keys_marked_as_stale.Get());
  crosapi_backend().FlushMojo();

  EXPECT_THAT(trusted_vault_client_ash().keys_marked_as_stale_count(), Eq(1));
}

TEST_F(CrosapiTrustedVaultClientTest, ShouldGetIsRecoverabilityDegraded) {
  trusted_vault_client_ash().SetIsRecoveryMethodRequired(true);

  base::MockCallback<base::OnceCallback<void(bool)>>
      on_get_is_recoverability_degraded;
  EXPECT_CALL(on_get_is_recoverability_degraded, Run(true));
  trusted_vault_client_lacros().GetIsRecoverabilityDegraded(
      primary_account_info(), on_get_is_recoverability_degraded.Get());

  // Getting degraded recoverability state is quite asynchronous in this setup:
  // 1. Ensure mojo propagates remote GetIsRecoverabilityDegraded() call.
  crosapi_backend().FlushMojo();
  // 2. Mimics asynchronous GetIsRecoverabilityDegraded() completion on
  // trusted_vault_client_ash() side.
  EXPECT_TRUE(trusted_vault_client_ash().CompleteAllPendingRequests());
  // 3. Ensure mojo propagates callback call.
  crosapi_backend().FlushMojo();
}

TEST_F(CrosapiTrustedVaultClientTest, ShouldAddTrustedRecoveryMethod) {
  const std::vector<uint8_t> recovery_method_public_key = {1, 2, 3, 4};
  const int recovery_method_type_hint = 4;

  base::MockCallback<base::OnceCallback<void()>> on_recovery_method_added;
  EXPECT_CALL(on_recovery_method_added, Run());
  trusted_vault_client_lacros().AddTrustedRecoveryMethod(
      primary_account_info().gaia, recovery_method_public_key,
      recovery_method_type_hint, on_recovery_method_added.Get());
  crosapi_backend().FlushMojo();

  const auto recovery_methods =
      trusted_vault_client_ash().server()->GetRecoveryMethods(
          primary_account_info().gaia);
  ASSERT_THAT(recovery_methods, SizeIs(1));
  EXPECT_THAT(recovery_methods[0].public_key, Eq(recovery_method_public_key));
  EXPECT_THAT(recovery_methods[0].method_type_hint,
              Eq(recovery_method_type_hint));
}

TEST_F(CrosapiTrustedVaultClientTest, ShouldClearLocalDataForAccount) {
  trusted_vault_client_ash().StoreKeys(primary_account_info().gaia,
                                       /*keys=*/{{1, 2, 3}},
                                       /*last_key_version=*/1);

  trusted_vault_client_lacros().ClearLocalDataForAccount(
      primary_account_info());
  crosapi_backend().FlushMojo();
  EXPECT_THAT(
      trusted_vault_client_ash().GetStoredKeys(primary_account_info().gaia),
      IsEmpty());
}

TEST_F(CrosapiTrustedVaultClientTest, ShouldNotifyObservers) {
  testing::NiceMock<MockTrustedVaultObserver> observer;
  trusted_vault_client_lacros().AddObserver(&observer);

  EXPECT_CALL(observer, OnTrustedVaultKeysChanged);
  trusted_vault_client_ash().StoreKeys(primary_account_info().gaia,
                                       /*keys=*/{{1, 2, 3}},
                                       /*last_key_version=*/1);
  crosapi_backend().FlushMojo();
  EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(&observer));

  EXPECT_CALL(observer, OnTrustedVaultRecoverabilityChanged);
  trusted_vault_client_ash().SetIsRecoveryMethodRequired(true);
  crosapi_backend().FlushMojo();

  trusted_vault_client_lacros().RemoveObserver(&observer);
}

}  // namespace
