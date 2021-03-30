// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system/user_removal_manager.h"

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/test_mock_time_task_runner.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

class FakeChromeUserRemovalManager : public FakeChromeUserManager {
 public:
  FakeChromeUserRemovalManager() = default;

  void RemoveUser(const AccountId& account_id,
                  user_manager::RemoveUserDelegate* delegate) override {
    RemoveUserFromList(account_id);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeChromeUserRemovalManager);
};

class UserRemovalManagerTest : public testing::Test {
 protected:
  UserRemovalManagerTest();
  ~UserRemovalManagerTest() override;

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;

  const ScopedTestingLocalState local_state_;
  const user_manager::ScopedUserManager scoped_user_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(UserRemovalManagerTest);
};

UserRemovalManagerTest::UserRemovalManagerTest()
    : task_runner_(base::MakeRefCounted<base::TestMockTimeTaskRunner>(
          base::TestMockTimeTaskRunner::Type::kBoundToThread)),
      local_state_(TestingBrowserProcess::GetGlobal()),
      scoped_user_manager_(std::make_unique<FakeChromeUserRemovalManager>()) {}

UserRemovalManagerTest::~UserRemovalManagerTest() = default;

}  // namespace

// Test that the InitiateUserRemoval/RemoveUsersIfNeeded sequence results in
// users being removed from the device.
TEST_F(UserRemovalManagerTest, TestUserRemovingWorks) {
  FakeChromeUserManager* fake_user_manager =
      static_cast<FakeChromeUserManager*>(user_manager::UserManager::Get());
  fake_user_manager->AddUser(AccountId::FromUserEmailGaiaId("user1", "1"));
  fake_user_manager->AddUser(AccountId::FromUserEmailGaiaId("user2", "2"));
  fake_user_manager->AddUser(AccountId::FromUserEmailGaiaId("user3", "3"));
  fake_user_manager->AddPublicAccountUser(
      AccountId::FromUserEmailGaiaId("public1", "4"));

  user_removal_manager::InitiateUserRemoval(base::OnceClosure());
  EXPECT_TRUE(user_removal_manager::RemoveUsersIfNeeded());
  EXPECT_TRUE(fake_user_manager->GetUsers().empty());
}

// Test that the failsafe timer runs LogOut after 60 seconds.
TEST_F(UserRemovalManagerTest, TestFailsafeTimer) {
  bool log_out_called = false;
  user_removal_manager::OverrideLogOutForTesting(base::BindOnce(
      [](bool* log_out_called) { *log_out_called = true; }, &log_out_called));

  // This call starts the timer.
  user_removal_manager::InitiateUserRemoval(base::OnceClosure());

  // After 55s the timer is not run yet.
  task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(55));
  EXPECT_FALSE(log_out_called);

  // After 60s the timer is run.
  task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(5));
  EXPECT_TRUE(log_out_called);
}

}  // namespace ash
