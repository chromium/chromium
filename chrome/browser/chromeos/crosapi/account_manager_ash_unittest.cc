// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crosapi/account_manager_ash.h"

#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chromeos/crosapi/mojom/account_manager.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/account_manager.mojom.h"
#include "components/account_manager_core/account_manager_util.h"
#include "components/prefs/testing_pref_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crosapi {

namespace {

const char kFakeGaiaId[] = "fake-gaia-id";
const char kFakeEmail[] = "fake_email@example.com";
const char kFakeToken[] = "fake-token";

}  // namespace

class TestAccountManagerObserver
    : public mojom::AccountManagerObserverInterceptorForTesting {
 public:
  TestAccountManagerObserver() : receiver_(this) {}

  TestAccountManagerObserver(const TestAccountManagerObserver&) = delete;
  TestAccountManagerObserver& operator=(const TestAccountManagerObserver&) =
      delete;
  ~TestAccountManagerObserver() override = default;

  void Observe(
      mojom::AccountManagerAsyncWaiter* const account_manager_async_waiter) {
    mojo::PendingReceiver<mojom::AccountManagerObserver> receiver;
    account_manager_async_waiter->AddObserver(&receiver);
    receiver_.Bind(std::move(receiver));
  }

  int GetNumOnTokenUpsertedCalls() { return num_token_upserted_calls_; }

  account_manager::Account GetLastUpsertedAccount() {
    return last_upserted_account_;
  }

  int GetNumOnAccountRemovedCalls() { return num_account_removed_calls_; }

  account_manager::Account GetLastRemovedAccount() {
    return last_removed_account_;
  }

 private:
  // mojom::AccountManagerObserverInterceptorForTesting override:
  AccountManagerObserver* GetForwardingInterface() override { return this; }

  // mojom::AccountManagerObserverInterceptorForTesting override:
  void OnTokenUpserted(mojom::AccountPtr account) override {
    ++num_token_upserted_calls_;
    last_upserted_account_ = account_manager::FromMojoAccount(account).value();
  }

  // mojom::AccountManagerObserverInterceptorForTesting override:
  void OnAccountRemoved(mojom::AccountPtr account) override {
    ++num_account_removed_calls_;
    last_removed_account_ = account_manager::FromMojoAccount(account).value();
  }

  int num_token_upserted_calls_ = 0;
  int num_account_removed_calls_ = 0;
  account_manager::Account last_upserted_account_;
  account_manager::Account last_removed_account_;
  mojo::Receiver<mojom::AccountManagerObserver> receiver_;
};

class AccountManagerAshTest : public ::testing::Test {
 public:
  AccountManagerAshTest() = default;
  AccountManagerAshTest(const AccountManagerAshTest&) = delete;
  AccountManagerAshTest& operator=(const AccountManagerAshTest&) = delete;
  ~AccountManagerAshTest() override = default;

 protected:
  void SetUp() override {
    account_manager_ash_ =
        std::make_unique<AccountManagerAsh>(&account_manager_);
    account_manager_ash_->BindReceiver(remote_.BindNewPipeAndPassReceiver());
    account_manager_async_waiter_ =
        std::make_unique<mojom::AccountManagerAsyncWaiter>(
            account_manager_ash_.get());
  }

  void RunAllPendingTasks() { task_environment_.RunUntilIdle(); }

  void FlushMojoForTesting() { account_manager_ash_->FlushMojoForTesting(); }

  // Returns |true| if initialization was successful.
  bool InitializeAccountManager() {
    base::RunLoop run_loop;
    account_manager_.InitializeInEphemeralMode(
        test_url_loader_factory_.GetSafeWeakWrapper(), run_loop.QuitClosure());
    account_manager_.SetPrefService(&pref_service_);
    account_manager_.RegisterPrefs(pref_service_.registry());
    run_loop.Run();
    return account_manager_.IsInitialized();
  }

  size_t GetNumObservers() { return account_manager_ash_->observers_.size(); }

  mojom::AccountManagerAsyncWaiter* account_manager_async_waiter() {
    return account_manager_async_waiter_.get();
  }

  chromeos::AccountManager* account_manager() { return &account_manager_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  TestingPrefServiceSimple pref_service_;
  chromeos::AccountManager account_manager_;
  mojo::Remote<mojom::AccountManager> remote_;
  std::unique_ptr<AccountManagerAsh> account_manager_ash_;
  std::unique_ptr<mojom::AccountManagerAsyncWaiter>
      account_manager_async_waiter_;
};

TEST_F(AccountManagerAshTest,
       IsInitializedReturnsFalseForUninitializedAccountManager) {
  bool is_initialized = true;
  account_manager_async_waiter()->IsInitialized(&is_initialized);
  EXPECT_FALSE(is_initialized);
}

TEST_F(AccountManagerAshTest,
       IsInitializedReturnsTrueForInitializedAccountManager) {
  bool is_initialized = true;
  account_manager_async_waiter()->IsInitialized(&is_initialized);
  EXPECT_FALSE(is_initialized);
  ASSERT_TRUE(InitializeAccountManager());
  account_manager_async_waiter()->IsInitialized(&is_initialized);
  EXPECT_TRUE(is_initialized);
}

// Test that lacros remotes do not leak.
TEST_F(AccountManagerAshTest,
       LacrosRemotesAreAutomaticallyRemovedOnConnectionClose) {
  EXPECT_EQ(0, GetNumObservers());
  {
    mojo::PendingReceiver<mojom::AccountManagerObserver> receiver;
    account_manager_async_waiter()->AddObserver(&receiver);
    EXPECT_EQ(1, GetNumObservers());
  }
  // Wait for the disconnect handler to be called.
  RunAllPendingTasks();
  EXPECT_EQ(0, GetNumObservers());
}

TEST_F(AccountManagerAshTest, LacrosObserversAreNotifiedOnAccountUpdates) {
  const account_manager::AccountKey kTestAccountKey{
      kFakeGaiaId, account_manager::AccountType::kGaia};
  ASSERT_TRUE(InitializeAccountManager());
  TestAccountManagerObserver observer;
  observer.Observe(account_manager_async_waiter());
  ASSERT_EQ(1, GetNumObservers());

  EXPECT_EQ(0, observer.GetNumOnTokenUpsertedCalls());
  account_manager()->UpsertAccount(kTestAccountKey, kFakeEmail, kFakeToken);
  FlushMojoForTesting();
  EXPECT_EQ(1, observer.GetNumOnTokenUpsertedCalls());
  EXPECT_EQ(kTestAccountKey, observer.GetLastUpsertedAccount().key);
  EXPECT_EQ(kFakeEmail, observer.GetLastUpsertedAccount().raw_email);
}

TEST_F(AccountManagerAshTest, LacrosObserversAreNotifiedOnAccountRemovals) {
  const account_manager::AccountKey kTestAccountKey{
      kFakeGaiaId, account_manager::AccountType::kGaia};
  ASSERT_TRUE(InitializeAccountManager());
  TestAccountManagerObserver observer;
  observer.Observe(account_manager_async_waiter());
  ASSERT_EQ(1, GetNumObservers());
  account_manager()->UpsertAccount(kTestAccountKey, kFakeEmail, kFakeToken);
  FlushMojoForTesting();

  EXPECT_EQ(0, observer.GetNumOnAccountRemovedCalls());
  account_manager()->RemoveAccount(kTestAccountKey);
  FlushMojoForTesting();
  EXPECT_EQ(1, observer.GetNumOnAccountRemovedCalls());
  EXPECT_EQ(kTestAccountKey, observer.GetLastRemovedAccount().key);
  EXPECT_EQ(kFakeEmail, observer.GetLastRemovedAccount().raw_email);
}

}  // namespace crosapi
