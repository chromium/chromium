// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crosapi/account_manager_ash.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "chromeos/components/account_manager/account_manager.h"
#include "chromeos/crosapi/mojom/account_manager.mojom-test-utils.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crosapi {

class AccountManagerAshTest : public ::testing::Test {
 public:
  AccountManagerAshTest() = default;
  AccountManagerAshTest(const AccountManagerAshTest&) = delete;
  AccountManagerAshTest& operator=(const AccountManagerAshTest&) = delete;
  ~AccountManagerAshTest() override = default;

 protected:
  void SetUp() override {
    account_manager_ash_ = std::make_unique<AccountManagerAsh>(
        &account_manager_, remote_.BindNewPipeAndPassReceiver());
    account_manager_async_waiter_ =
        std::make_unique<mojom::AccountManagerAsyncWaiter>(
            account_manager_ash_.get());
  }

  // Returns |true| if initialization was successful.
  bool InitializeAccountManager() {
    base::RunLoop run_loop;
    account_manager_.InitializeInEphemeralMode(
        test_url_loader_factory_.GetSafeWeakWrapper(), run_loop.QuitClosure());
    run_loop.Run();
    return account_manager_.IsInitialized();
  }

  mojom::AccountManagerAsyncWaiter* account_manager_async_waiter() {
    return account_manager_async_waiter_.get();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  network::TestURLLoaderFactory test_url_loader_factory_;
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

}  // namespace crosapi
