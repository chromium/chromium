// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system/user_removal_manager.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/test_mock_time_task_runner.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

class UserRemovalManagerTest : public testing::Test {
 public:
  UserRemovalManagerTest(const UserRemovalManagerTest&) = delete;
  UserRemovalManagerTest& operator=(const UserRemovalManagerTest&) = delete;

 protected:
  UserRemovalManagerTest();
  ~UserRemovalManagerTest() override;

  FakeChromeUserManager* fake_user_manager() { return user_manager_.Get(); }

  void SetUp() override {
    testing::Test::SetUp();
    fake_user_manager()->AddUser(AccountId::FromUserEmailGaiaId("user1", "1"));
    fake_user_manager()->AddUser(AccountId::FromUserEmailGaiaId("user2", "2"));
    fake_user_manager()->AddUser(AccountId::FromUserEmailGaiaId("user3", "3"));
  }

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  ScopedTestingLocalState local_state_;
  ash::ScopedStubInstallAttributes install_attributes_{
      ash::StubInstallAttributes::CreateCloudManaged("test.domain",
                                                     "device_id")};
  ash::ScopedTestingCrosSettings cros_settings_;
  user_manager::TypedScopedUserManager<FakeChromeUserManager> user_manager_{
      std::make_unique<FakeChromeUserManager>()};
};

UserRemovalManagerTest::UserRemovalManagerTest()
    : task_runner_(base::MakeRefCounted<base::TestMockTimeTaskRunner>(
          base::TestMockTimeTaskRunner::Type::kBoundToThread)),
      local_state_(TestingBrowserProcess::GetGlobal()) {}

UserRemovalManagerTest::~UserRemovalManagerTest() = default;

}  // namespace

// Test that the InitiateUserRemoval/RemoveUsersIfNeeded sequence results in
// users being removed from the device.
TEST_F(UserRemovalManagerTest, TestUserRemovingWorks) {
  user_removal_manager::InitiateUserRemoval(base::OnceClosure());
  EXPECT_TRUE(user_removal_manager::RemoveUsersIfNeeded());
  EXPECT_TRUE(fake_user_manager()->GetUsers().empty());
  EXPECT_TRUE(local_state_.Get()
                  ->FindPreference(prefs::kRemoveUsersRemoteCommand)
                  ->IsDefaultValue());
}

// Test that if Chrome crashes in the middle of the users removal - it does not
// try again.
TEST_F(UserRemovalManagerTest, TestUserRemovingDoNotRetryOnFailure) {
  // If explicitly set to false - it means chrome might've crashed during the
  // previous removal.
  local_state_.Get()->SetBoolean(prefs::kRemoveUsersRemoteCommand, false);
  EXPECT_FALSE(user_removal_manager::RemoveUsersIfNeeded());
  EXPECT_FALSE(fake_user_manager()->GetUsers().empty());
}

// Test that the failsafe timer runs LogOut after 60 seconds.
TEST_F(UserRemovalManagerTest, TestFailsafeTimer) {
  bool log_out_called = false;
  user_removal_manager::OverrideLogOutForTesting(base::BindOnce(
      [](bool* log_out_called) { *log_out_called = true; }, &log_out_called));

  // This call starts the timer.
  user_removal_manager::InitiateUserRemoval(base::OnceClosure());

  // After 55s the timer is not run yet.
  task_runner_->FastForwardBy(base::Seconds(55));
  EXPECT_FALSE(log_out_called);

  // After 60s the timer is run.
  task_runner_->FastForwardBy(base::Seconds(5));
  EXPECT_TRUE(log_out_called);
}

}  // namespace ash
