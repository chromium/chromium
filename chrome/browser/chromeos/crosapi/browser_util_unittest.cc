// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crosapi/browser_util.h"

#include "components/account_id/account_id.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/version_info/channel.h"
#include "testing/gtest/include/gtest/gtest.h"

using user_manager::User;
using version_info::Channel;

namespace crosapi {

class LacrosUtilTest : public testing::Test {
 public:
  LacrosUtilTest() = default;
  ~LacrosUtilTest() override = default;

  void SetUp() override {
    auto fake_user_manager = std::make_unique<user_manager::FakeUserManager>();
    fake_user_manager_ = fake_user_manager.get();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));
  }

  void AddRegularUser(const std::string& email) {
    AccountId account_id = AccountId::FromUserEmail(email);
    const User* user = fake_user_manager_->AddUser(account_id);
    fake_user_manager_->UserLoggedIn(account_id, user->username_hash(),
                                     /*browser_restart=*/false,
                                     /*is_child=*/false);
  }

  user_manager::FakeUserManager* fake_user_manager_ = nullptr;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
};

TEST_F(LacrosUtilTest, AllowedForDeveloperBuild) {
  AddRegularUser("user@test.com");
  EXPECT_TRUE(browser_util::IsLacrosAllowed(Channel::UNKNOWN));
}

TEST_F(LacrosUtilTest, BlockedForRegularUser) {
  AddRegularUser("user@test.com");
  EXPECT_FALSE(browser_util::IsLacrosAllowed(Channel::CANARY));
  EXPECT_FALSE(browser_util::IsLacrosAllowed(Channel::DEV));
  EXPECT_FALSE(browser_util::IsLacrosAllowed(Channel::BETA));
  EXPECT_FALSE(browser_util::IsLacrosAllowed(Channel::STABLE));
}

TEST_F(LacrosUtilTest, AllowedForGoogler) {
  AddRegularUser("user@google.com");
  EXPECT_TRUE(browser_util::IsLacrosAllowed(Channel::CANARY));
  EXPECT_TRUE(browser_util::IsLacrosAllowed(Channel::DEV));
  EXPECT_TRUE(browser_util::IsLacrosAllowed(Channel::BETA));
}

TEST_F(LacrosUtilTest, BlockedForGoogler) {
  AddRegularUser("user@google.com");
  EXPECT_FALSE(browser_util::IsLacrosAllowed(Channel::STABLE));
}

TEST_F(LacrosUtilTest, BlockedForChildUser) {
  AccountId account_id = AccountId::FromUserEmail("user@test.com");
  const User* user = fake_user_manager_->AddChildUser(account_id);
  fake_user_manager_->UserLoggedIn(account_id, user->username_hash(),
                                   /*browser_restart=*/false,
                                   /*is_child=*/true);
  EXPECT_FALSE(browser_util::IsLacrosAllowed(Channel::UNKNOWN));
}

}  // namespace crosapi
