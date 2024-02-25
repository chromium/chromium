// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/trusted_vault/trusted_vault_backend_service_ash.h"

#include <memory>

#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chromeos/crosapi/mojom/account_manager.mojom.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/trusted_vault/test/fake_trusted_vault_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using testing::Eq;
using testing::IsEmpty;
using testing::SizeIs;

class TestTrustedVaultBackendObserver
    : public crosapi::mojom::TrustedVaultBackendObserver {
 public:
  TestTrustedVaultBackendObserver(
      mojo::Remote<crosapi::mojom::TrustedVaultBackend>* backend) {
    backend->get()->AddObserver(receiver_.BindNewPipeAndPassRemote());
    backend->FlushForTesting();
  }

  ~TestTrustedVaultBackendObserver() override = default;

  int num_on_trusted_vault_keys_changed_calls() const {
    return num_on_trusted_vault_keys_changed_calls_;
  }

  int num_on_trusted_vault_recoverability_changed_calls() const {
    return num_on_trusted_vault_recoverability_changed_calls_;
  }

  void OnTrustedVaultKeysChanged() override {
    num_on_trusted_vault_keys_changed_calls_++;
  }

  void OnTrustedVaultRecoverabilityChanged() override {
    num_on_trusted_vault_recoverability_changed_calls_++;
  }

 private:
  mojo::Receiver<crosapi::mojom::TrustedVaultBackendObserver> receiver_{this};

  int num_on_trusted_vault_keys_changed_calls_ = 0;
  int num_on_trusted_vault_recoverability_changed_calls_ = 0;
};

class TrustedVaultBackendServiceAshTest : public testing::Test {
 public:
  TrustedVaultBackendServiceAshTest() {
    primary_account_info_ = identity_test_env_.MakePrimaryAccountAvailable(
        "example@gmail.com", signin::ConsentLevel::kSignin);
    backend_ = std::make_unique<TrustedVaultBackendServiceAsh>(
        identity_test_env_.identity_manager(), &trusted_vault_client_ash_);
    backend_->BindReceiver(backend_remote_.BindNewPipeAndPassReceiver());
  }

  ~TrustedVaultBackendServiceAshTest() override = default;

  AccountInfo* primary_account_info() { return &primary_account_info_; }

  crosapi::mojom::AccountKeyPtr GetPrimaryAccountKey() const {
    crosapi::mojom::AccountKeyPtr account_key =
        crosapi::mojom::AccountKey::New();
    account_key->id = primary_account_info_.gaia;
    account_key->account_type = crosapi::mojom::AccountType::kGaia;
    return account_key;
  }

  crosapi::mojom::AccountKeyPtr GetNonPrimaryAccountKey() const {
    crosapi::mojom::AccountKeyPtr account_key = GetPrimaryAccountKey();
    account_key->id += "a";
    return account_key;
  }

  trusted_vault::FakeTrustedVaultClient* client_ash() {
    return &trusted_vault_client_ash_;
  }

  TrustedVaultBackendServiceAsh* backend() { return backend_.get(); }

  mojo::Remote<crosapi::mojom::TrustedVaultBackend>& backend_remote() {
    return backend_remote_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  signin::IdentityTestEnvironment identity_test_env_;
  AccountInfo primary_account_info_;

  trusted_vault::FakeTrustedVaultClient trusted_vault_client_ash_;
  std::unique_ptr<TrustedVaultBackendServiceAsh> backend_;
  mojo::Remote<crosapi::mojom::TrustedVaultBackend> backend_remote_;
};

TEST_F(TrustedVaultBackendServiceAshTest, ShouldFetchKeys) {
  const std::vector<std::vector<uint8_t>> keys = {{1, 2, 3}};
  client_ash()->StoreKeys(primary_account_info()->gaia, keys,
                          /*last_key_version=*/1);

  base::MockCallback<TrustedVaultBackendServiceAsh::FetchKeysCallback>
      on_keys_fetched;
  EXPECT_CALL(on_keys_fetched, Run(keys));
  backend_remote()->FetchKeys(GetPrimaryAccountKey(), on_keys_fetched.Get());
  // Fetching keys is quite asynchronous in this setup:
  // 1. Ensure mojo propagates remote FetchKeys() call.
  backend_remote().FlushForTesting();
  // 2. Mimics asynchronous fetch completion on client_ash() side.
  EXPECT_TRUE(client_ash()->CompleteAllPendingRequests());
  // 3. Ensure mojo propagates callback call.
  backend_remote().FlushForTesting();
}

TEST_F(TrustedVaultBackendServiceAshTest, ShouldValidateAccountKeyOnFetchKeys) {
  const std::vector<std::vector<uint8_t>> keys = {{1, 2, 3}};
  client_ash()->StoreKeys(primary_account_info()->gaia, keys,
                          /*last_key_version=*/1);

  base::MockCallback<TrustedVaultBackendServiceAsh::FetchKeysCallback>
      on_keys_fetched;
  EXPECT_CALL(on_keys_fetched, Run(IsEmpty()));
  backend_remote()->FetchKeys(GetNonPrimaryAccountKey(), on_keys_fetched.Get());
  backend_remote().FlushForTesting();
}

TEST(TrustedVaultBackendServiceAshNoFixtureTest,
     ShouldHandleAbsenseOfPrimaryAccount) {
  base::test::SingleThreadTaskEnvironment task_environment;
  signin::IdentityTestEnvironment identity_test_env;

  trusted_vault::FakeTrustedVaultClient trusted_vault_client_ash;
  std::unique_ptr<TrustedVaultBackendServiceAsh> backend =
      std::make_unique<TrustedVaultBackendServiceAsh>(
          identity_test_env.identity_manager(), &trusted_vault_client_ash);

  mojo::Remote<crosapi::mojom::TrustedVaultBackend> backend_remote;
  backend->BindReceiver(backend_remote.BindNewPipeAndPassReceiver());

  ASSERT_FALSE(identity_test_env.identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));

  // Mimic that some data is stored for `gaia`. This shouldn't be possible when
  // there is no primary account, but this test does this to make meaningful
  // expectations.
  const std::string gaia = "example";
  const std::vector<std::vector<uint8_t>> keys = {{1, 2, 3}};
  trusted_vault_client_ash.StoreKeys(gaia, keys, /*last_key_version=*/1);

  crosapi::mojom::AccountKeyPtr account_key = crosapi::mojom::AccountKey::New();
  account_key->id = gaia;
  account_key->account_type = crosapi::mojom::AccountType::kGaia;

  base::MockCallback<TrustedVaultBackendServiceAsh::FetchKeysCallback>
      on_keys_fetched;
  EXPECT_CALL(on_keys_fetched, Run(IsEmpty()));
  backend_remote->FetchKeys(std::move(account_key), on_keys_fetched.Get());
  backend_remote.FlushForTesting();
}

TEST_F(TrustedVaultBackendServiceAshTest, ShouldMarkLocalKeysAsStale) {
  client_ash()->StoreKeys(primary_account_info()->gaia, /*keys=*/{{1, 2, 3}},
                          /*last_key_version=*/1);

  base::MockCallback<
      TrustedVaultBackendServiceAsh::MarkLocalKeysAsStaleCallback>
      on_keys_marked_as_stale;
  EXPECT_CALL(on_keys_marked_as_stale, Run(true));
  backend_remote()->MarkLocalKeysAsStale(GetPrimaryAccountKey(),
                                         on_keys_marked_as_stale.Get());
  backend_remote().FlushForTesting();

  EXPECT_THAT(client_ash()->keys_marked_as_stale_count(), Eq(1));
}

TEST_F(TrustedVaultBackendServiceAshTest,
       ShouldValidateAccountKeyOnMarkLocalKeysAsStale) {
  client_ash()->StoreKeys(primary_account_info()->gaia, /*keys=*/{{1, 2, 3}},
                          /*last_key_version=*/1);

  base::MockCallback<
      TrustedVaultBackendServiceAsh::MarkLocalKeysAsStaleCallback>
      on_keys_marked_as_stale;
  EXPECT_CALL(on_keys_marked_as_stale, Run(false));
  backend_remote()->MarkLocalKeysAsStale(GetNonPrimaryAccountKey(),
                                         on_keys_marked_as_stale.Get());
  backend_remote().FlushForTesting();

  EXPECT_THAT(client_ash()->keys_marked_as_stale_count(), Eq(0));
}

TEST_F(TrustedVaultBackendServiceAshTest, ShouldStoreKeys) {
  const std::vector<std::vector<uint8_t>> keys = {{1, 2, 3}};
  backend_remote()->StoreKeys(GetPrimaryAccountKey(), keys,
                              /*last_key_version=*/1);
  backend_remote().FlushForTesting();
  EXPECT_THAT(client_ash()->GetStoredKeys(primary_account_info()->gaia),
              Eq(keys));
}

TEST_F(TrustedVaultBackendServiceAshTest, ShouldGetIsRecoverabilityDegraded) {
  client_ash()->SetIsRecoveryMethodRequired(true);

  base::MockCallback<
      TrustedVaultBackendServiceAsh::GetIsRecoverabilityDegradedCallback>
      on_get_is_recoverability_degraded;
  EXPECT_CALL(on_get_is_recoverability_degraded, Run(true));
  backend_remote()->GetIsRecoverabilityDegraded(
      GetPrimaryAccountKey(), on_get_is_recoverability_degraded.Get());
  // Getting degraded recoverability state is quite asynchronous in this setup:
  // 1. Ensure mojo propagates remote GetIsRecoverabilityDegraded() call.
  backend_remote().FlushForTesting();
  // 2. Mimics asynchronous GetIsRecoverabilityDegraded() completion on
  // client_ash() side.
  EXPECT_TRUE(client_ash()->CompleteAllPendingRequests());
  // 3. Ensure mojo propagates callback call.
  backend_remote().FlushForTesting();
}

TEST_F(TrustedVaultBackendServiceAshTest,
       ShouldValidateAccountOnGetIsRecoverabilityDegraded) {
  client_ash()->SetIsRecoveryMethodRequired(true);

  base::MockCallback<
      TrustedVaultBackendServiceAsh::GetIsRecoverabilityDegradedCallback>
      on_get_is_recoverability_degraded;
  EXPECT_CALL(on_get_is_recoverability_degraded, Run(false));
  backend_remote()->GetIsRecoverabilityDegraded(
      GetNonPrimaryAccountKey(), on_get_is_recoverability_degraded.Get());
  backend_remote().FlushForTesting();
}

TEST_F(TrustedVaultBackendServiceAshTest, ShouldAddTrustedRecoveryMethod) {
  const std::vector<uint8_t> recovery_method_public_key = {1, 2, 3, 4};
  const int recovery_method_type_hint = 4;

  base::MockCallback<
      TrustedVaultBackendServiceAsh::AddTrustedRecoveryMethodCallback>
      on_recovery_method_added;
  EXPECT_CALL(on_recovery_method_added, Run());
  backend_remote()->AddTrustedRecoveryMethod(
      GetPrimaryAccountKey(), recovery_method_public_key,
      recovery_method_type_hint, on_recovery_method_added.Get());
  backend_remote().FlushForTesting();

  const auto recovery_methods =
      client_ash()->server()->GetRecoveryMethods(primary_account_info()->gaia);
  ASSERT_THAT(recovery_methods, SizeIs(1));
  EXPECT_THAT(recovery_methods[0].public_key, Eq(recovery_method_public_key));
  EXPECT_THAT(recovery_methods[0].method_type_hint,
              Eq(recovery_method_type_hint));
}

TEST_F(TrustedVaultBackendServiceAshTest,
       ShouldValidateAccountOnAddTrustedRecoveryMethod) {
  base::MockCallback<
      TrustedVaultBackendServiceAsh::AddTrustedRecoveryMethodCallback>
      on_recovery_method_added;
  EXPECT_CALL(on_recovery_method_added, Run());
  backend_remote()->AddTrustedRecoveryMethod(
      GetNonPrimaryAccountKey(), /*public_key=*/{1, 2, 3, 4},
      /*method_type_hint=*/1, on_recovery_method_added.Get());
  backend_remote().FlushForTesting();

  EXPECT_THAT(
      client_ash()->server()->GetRecoveryMethods(primary_account_info()->gaia),
      IsEmpty());
}

TEST_F(TrustedVaultBackendServiceAshTest, ShouldClearLocalDataForAccount) {
  client_ash()->StoreKeys(primary_account_info()->gaia, /*keys=*/{{1, 2, 3}},
                          /*last_key_version=*/1);

  backend_remote()->ClearLocalDataForAccount(GetPrimaryAccountKey());
  backend_remote().FlushForTesting();
  EXPECT_THAT(client_ash()->GetStoredKeys(primary_account_info()->gaia),
              IsEmpty());
}

TEST_F(TrustedVaultBackendServiceAshTest,
       ShouldValidateAccountInfoOnClearLocalDataForAccount) {
  const std::vector<std::vector<uint8_t>> keys = {{1, 2, 3}};
  client_ash()->StoreKeys(primary_account_info()->gaia, keys,
                          /*last_key_version=*/1);

  backend_remote()->ClearLocalDataForAccount(GetNonPrimaryAccountKey());
  backend_remote().FlushForTesting();
  EXPECT_THAT(client_ash()->GetStoredKeys(primary_account_info()->gaia),
              Eq(keys));
}

TEST_F(TrustedVaultBackendServiceAshTest, ShouldNotifyObservers) {
  TestTrustedVaultBackendObserver observer(&backend_remote());
  client_ash()->StoreKeys(primary_account_info()->gaia, /*keys=*/{{1, 2, 3}},
                          /*last_key_version=*/1);
  backend_remote().FlushForTesting();
  EXPECT_THAT(observer.num_on_trusted_vault_keys_changed_calls(), Eq(1));
  EXPECT_THAT(observer.num_on_trusted_vault_recoverability_changed_calls(),
              Eq(0));

  client_ash()->SetIsRecoveryMethodRequired(true);
  backend_remote().FlushForTesting();
  EXPECT_THAT(observer.num_on_trusted_vault_recoverability_changed_calls(),
              Eq(1));
  EXPECT_THAT(observer.num_on_trusted_vault_keys_changed_calls(), Eq(1));
}

TEST_F(TrustedVaultBackendServiceAshTest, ShouldSupportMultipleObservers) {
  TestTrustedVaultBackendObserver observer1(&backend_remote());
  TestTrustedVaultBackendObserver observer2(&backend_remote());
  client_ash()->StoreKeys(primary_account_info()->gaia, /*keys=*/{{1, 2, 3}},
                          /*last_key_version=*/1);
  backend_remote().FlushForTesting();
  EXPECT_THAT(observer1.num_on_trusted_vault_keys_changed_calls(), Eq(1));
  EXPECT_THAT(observer2.num_on_trusted_vault_keys_changed_calls(), Eq(1));
}

TEST_F(TrustedVaultBackendServiceAshTest, ShouldSupportMultipleRemotes) {
  mojo::Remote<crosapi::mojom::TrustedVaultBackend> secondary_remote;
  backend()->BindReceiver(secondary_remote.BindNewPipeAndPassReceiver());

  // Store keys using first remote, should succeed even though
  // `secondary_remote` was bound after it.
  const std::vector<std::vector<uint8_t>> keys = {{1, 2, 3}};
  backend_remote()->StoreKeys(GetPrimaryAccountKey(), keys,
                              /*last_key_version=*/1);
  backend_remote().FlushForTesting();
  EXPECT_THAT(client_ash()->GetStoredKeys(primary_account_info()->gaia),
              Eq(keys));

  // Verify that `secondary_remote` able to store keys too.
  const std::vector<std::vector<uint8_t>> other_keys = {{1, 2, 3, 4}};
  backend_remote()->StoreKeys(GetPrimaryAccountKey(), other_keys,
                              /*last_key_version=*/2);
  backend_remote().FlushForTesting();
  EXPECT_THAT(client_ash()->GetStoredKeys(primary_account_info()->gaia),
              Eq(other_keys));
}

TEST_F(TrustedVaultBackendServiceAshTest, ShouldDisconnectOnShutdown) {
  ASSERT_TRUE(backend_remote().is_connected());
  backend()->Shutdown();
  backend_remote().FlushForTesting();
  EXPECT_FALSE(backend_remote().is_connected());
}

}  // namespace ash
