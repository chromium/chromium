// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/device_ownership_waiter_impl.h"

#include <memory>

#include "base/check_deref.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crosapi {

class DeviceOwnershipWaiterTest : public testing::Test {
 public:
  DeviceOwnershipWaiterTest() = default;
  ~DeviceOwnershipWaiterTest() override = default;

  void SetUp() override {
    auto user_manager = std::make_unique<ash::FakeChromeUserManager>();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));
  }

  ash::FakeChromeUserManager& GetFakeUserManager() {
    return CHECK_DEREF(static_cast<ash::FakeChromeUserManager*>(
        user_manager::UserManager::Get()));
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
};

TEST_F(DeviceOwnershipWaiterTest, DelaysCorrectly) {
  {
    DeviceOwnershipWaiterImpl waiter;

    base::test::TestFuture<void> future;
    waiter.WaitForOwnerhipFetched(future.GetCallback(),
                                  /*launching_at_login_screen=*/true);

    GetFakeUserManager().SetOwnerId(user_manager::StubAccountId());

    EXPECT_TRUE(future.Wait());
  }
  {
    DeviceOwnershipWaiterImpl waiter;

    base::test::TestFuture<void> future;
    waiter.WaitForOwnerhipFetched(future.GetCallback(),
                                  /*launching_at_login_screen=*/false);

    GetFakeUserManager().SetOwnerId(user_manager::StubAccountId());

    EXPECT_TRUE(future.Wait());
  }
}

}  // namespace crosapi
