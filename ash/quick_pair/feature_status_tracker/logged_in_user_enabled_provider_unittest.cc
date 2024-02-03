// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/feature_status_tracker/logged_in_user_enabled_provider.h"

#include <memory>

#include "ash/test/ash_test_base.h"
#include "components/user_manager/user_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace quick_pair {

const std::string kUserEmail = "test@test.test";

class LoggedInUserEnabledProviderTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();
    ClearLogin();

    provider_ = std::make_unique<LoggedInUserEnabledProvider>();
  }

  void TearDown() override {
    provider_.reset();

    AshTestBase::TearDown();
  }

 protected:
  void Login(user_manager::UserType user_type) {
    SimulateUserLogin(kUserEmail, user_type);
  }

  std::unique_ptr<LoggedInUserEnabledProvider> provider_;
};

TEST_F(LoggedInUserEnabledProviderTest, NotLoggedIn) {
  EXPECT_FALSE(provider_->is_enabled());
}

TEST_F(LoggedInUserEnabledProviderTest, Locked) {
  GetSessionControllerClient()->LockScreen();
  EXPECT_FALSE(provider_->is_enabled());
}

TEST_F(LoggedInUserEnabledProviderTest, LockAndUnlock) {
  GetSessionControllerClient()->LockScreen();
  EXPECT_FALSE(provider_->is_enabled());
  GetSessionControllerClient()->UnlockScreen();
  EXPECT_TRUE(provider_->is_enabled());
}

TEST_F(LoggedInUserEnabledProviderTest, Kiosk) {
  Login(user_manager::UserType::kKioskApp);
  EXPECT_FALSE(provider_->is_enabled());
}

TEST_F(LoggedInUserEnabledProviderTest, UserLoggedIn) {
  Login(user_manager::UserType::kRegular);
  EXPECT_TRUE(provider_->is_enabled());
}

TEST_F(LoggedInUserEnabledProviderTest, GuestLoggedIn) {
  Login(user_manager::UserType::kGuest);
  EXPECT_TRUE(provider_->is_enabled());
}

TEST_F(LoggedInUserEnabledProviderTest, PublicAccountLoggedIn) {
  Login(user_manager::UserType::kPublicAccount);
  EXPECT_FALSE(provider_->is_enabled());
}

TEST_F(LoggedInUserEnabledProviderTest, ChildLoggedIn) {
  Login(user_manager::UserType::kChild);
  EXPECT_TRUE(provider_->is_enabled());
}

}  // namespace quick_pair
}  // namespace ash
