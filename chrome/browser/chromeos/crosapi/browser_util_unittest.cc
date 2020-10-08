// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crosapi/browser_util.h"

#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/version_info/channel.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using user_manager::User;
using version_info::Channel;

namespace crosapi {

class LacrosUtilTest : public testing::Test {
 public:
  LacrosUtilTest() = default;
  ~LacrosUtilTest() override = default;

  void SetUp() override {
    fake_user_manager_ = new chromeos::FakeChromeUserManager;
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        base::WrapUnique(fake_user_manager_));
  }

  void AddRegularUser(const std::string& email) {
    AccountId account_id = AccountId::FromUserEmail(email);
    const User* user = fake_user_manager_->AddUser(account_id);
    fake_user_manager_->UserLoggedIn(account_id, user->username_hash(),
                                     /*browser_restart=*/false,
                                     /*is_child=*/false);
    chromeos::ProfileHelper::Get()->SetUserToProfileMappingForTesting(
        user, &testing_profile_);
  }

  // The order of these members is relevant for both construction and
  // destruction timing.
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile testing_profile_;
  chromeos::FakeChromeUserManager* fake_user_manager_ = nullptr;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
};

TEST_F(LacrosUtilTest, ChannelTest) {
  AddRegularUser("user@test.com");

  EXPECT_TRUE(browser_util::IsLacrosAllowed(Channel::UNKNOWN));
  EXPECT_TRUE(browser_util::IsLacrosAllowed(Channel::CANARY));
  EXPECT_TRUE(browser_util::IsLacrosAllowed(Channel::DEV));
  EXPECT_TRUE(browser_util::IsLacrosAllowed(Channel::BETA));
  EXPECT_FALSE(browser_util::IsLacrosAllowed(Channel::STABLE));
}

TEST_F(LacrosUtilTest, ManagedAccountGoogle) {
  AddRegularUser("user@google.com");
  testing_profile_.GetProfilePolicyConnector()->OverrideIsManagedForTesting(
      true);

  EXPECT_TRUE(browser_util::IsLacrosAllowed(Channel::CANARY));
}

TEST_F(LacrosUtilTest, ManagedAccountFakeGoogle) {
  AddRegularUser("user@thisisnotgoogle.com");
  testing_profile_.GetProfilePolicyConnector()->OverrideIsManagedForTesting(
      true);

  EXPECT_FALSE(browser_util::IsLacrosAllowed(Channel::CANARY));
}

TEST_F(LacrosUtilTest, ManagedAccountNonGoogle) {
  AddRegularUser("user@foople.com");
  testing_profile_.GetProfilePolicyConnector()->OverrideIsManagedForTesting(
      true);

  EXPECT_FALSE(browser_util::IsLacrosAllowed(Channel::CANARY));
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
