// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/primary_profile_creation_waiter.h"

#include <memory>

#include "base/test/test_future.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crosapi {

namespace {

const char kPrimaryProfileEmail[] = "user@test";

class PrimaryProfileCreationWaiterTest : public testing::Test {
 protected:
  void SetUp() override {
    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());
    testing_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(testing_profile_manager_->SetUp());
  }

  void TearDown() override {
    // Ensure the observer (Waiter) is reset before the source (ProfileManager).
    primary_profile_creation_waiter_.reset();
    testing_profile_manager_.reset();
  }

  content::BrowserTaskEnvironment task_environment_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  std::unique_ptr<PrimaryProfileCreationWaiter>
      primary_profile_creation_waiter_;
};

TEST_F(PrimaryProfileCreationWaiterTest,
       RunCallbackImmediatelyIfPrimaryProfileIsCreated) {
  // Add an user, log in, and set profile creation as complete.
  const AccountId account_id(AccountId::FromUserEmail(kPrimaryProfileEmail));
  fake_user_manager_->AddUser(account_id);
  fake_user_manager_->LoginUser(account_id, /*set_profile_created_flag=*/true);

  // Initialize the waiter.
  base::test::TestFuture<void> future;
  primary_profile_creation_waiter_ = PrimaryProfileCreationWaiter::WaitOrRun(
      testing_profile_manager_->profile_manager(), future.GetCallback());

  // The callback has already been called.
  EXPECT_TRUE(future.IsReady());
  // No waiter was instantiated, as it wasn't needed.
  EXPECT_FALSE(primary_profile_creation_waiter_);
}

TEST_F(PrimaryProfileCreationWaiterTest,
       RunCallbackWhenPrimaryProfileIsCreated) {
  // Add an user and log in, but set profile creation as not complete yet.
  const AccountId account_id(AccountId::FromUserEmail(kPrimaryProfileEmail));
  fake_user_manager_->AddUser(account_id);
  fake_user_manager_->LoginUser(account_id, /*set_profile_created_flag=*/false);

  base::test::TestFuture<void> future;
  primary_profile_creation_waiter_ = PrimaryProfileCreationWaiter::WaitOrRun(
      testing_profile_manager_->profile_manager(), future.GetCallback());

  // Create the profile.
  testing_profile_manager_->CreateTestingProfile(account_id.GetUserEmail());

  // The waiter was instantiated.
  EXPECT_TRUE(primary_profile_creation_waiter_);
  // The callback gets called.
  EXPECT_TRUE(future.Wait());
}

}  // namespace

}  // namespace crosapi
