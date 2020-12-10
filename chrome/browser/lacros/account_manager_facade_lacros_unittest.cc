// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/account_manager_facade_lacros.h"

#include "base/test/task_environment.h"
#include "chromeos/crosapi/mojom/account_manager.mojom.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_manager_test_util.h"
#include "components/account_manager_core/account_manager_util.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class FakeAccountManager : public crosapi::mojom::AccountManager {
 public:
  FakeAccountManager() = default;
  FakeAccountManager(const FakeAccountManager&) = delete;
  FakeAccountManager& operator=(const FakeAccountManager&) = delete;
  ~FakeAccountManager() override = default;

  void IsInitialized(IsInitializedCallback cb) override {
    std::move(cb).Run(is_initialized_);
  }

  void SetIsInitialized(bool is_initialized) {
    is_initialized_ = is_initialized;
  }

  void AddObserver(AddObserverCallback cb) override {
    mojo::Remote<crosapi::mojom::AccountManagerObserver> observer;
    std::move(cb).Run(observer.BindNewPipeAndPassReceiver());
    observers_.Add(std::move(observer));
  }

  mojo::Remote<crosapi::mojom::AccountManager> CreateRemote() {
    mojo::Remote<crosapi::mojom::AccountManager> remote;
    receivers_.Add(this, remote.BindNewPipeAndPassReceiver());
    return remote;
  }

  void NotifyOnTokenUpsertedObservers(const account_manager::Account& account) {
    for (auto& observer : observers_) {
      observer->OnTokenUpserted(ToMojoAccount(account));
    }
  }

 private:
  bool is_initialized_{false};
  mojo::ReceiverSet<crosapi::mojom::AccountManager> receivers_;
  mojo::RemoteSet<crosapi::mojom::AccountManagerObserver> observers_;
};

}  // namespace

class AccountManagerFacadeLacrosTest : public testing::Test {
 public:
  AccountManagerFacadeLacrosTest() = default;
  AccountManagerFacadeLacrosTest(const AccountManagerFacadeLacrosTest&) =
      delete;
  AccountManagerFacadeLacrosTest& operator=(
      const AccountManagerFacadeLacrosTest&) = delete;
  ~AccountManagerFacadeLacrosTest() override = default;

 protected:
  FakeAccountManager& account_manager() { return account_manager_; }

  std::unique_ptr<AccountManagerFacadeLacros> CreateFacade() {
    base::RunLoop run_loop;
    auto result = std::make_unique<AccountManagerFacadeLacros>(
        account_manager().CreateRemote(), run_loop.QuitClosure());
    run_loop.Run();
    return result;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  FakeAccountManager account_manager_;
};

TEST_F(AccountManagerFacadeLacrosTest,
       FacadeIsInitializedOnConnectIfAccountManagerIsInitialized) {
  account_manager().SetIsInitialized(true);

  std::unique_ptr<AccountManagerFacadeLacros> account_manager_facade =
      CreateFacade();
  EXPECT_TRUE(account_manager_facade->IsInitialized());
}

TEST_F(AccountManagerFacadeLacrosTest, FacadeIsUninitializedByDefault) {
  std::unique_ptr<AccountManagerFacadeLacros> account_manager_facade =
      CreateFacade();
  EXPECT_FALSE(account_manager_facade->IsInitialized());
}

// TODO(https://crbug.com/1117472): Add more tests after implementing observers.
