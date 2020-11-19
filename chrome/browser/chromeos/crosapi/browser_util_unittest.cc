// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crosapi/browser_util.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
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
  LacrosUtilTest() : local_state_(TestingBrowserProcess::GetGlobal()) {}
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

  ScopedTestingLocalState local_state_;
};

TEST_F(LacrosUtilTest, LacrosEnabledByFlag) {
  AddRegularUser("user@test.com");

  // Lacros is disabled because the feature isn't enabled by default.
  EXPECT_FALSE(browser_util::IsLacrosEnabled());

  // Enabling the flag enables Lacros.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(chromeos::features::kLacrosSupport);
  EXPECT_TRUE(browser_util::IsLacrosEnabled());
}

TEST_F(LacrosUtilTest, ChannelTest) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(chromeos::features::kLacrosSupport);
  AddRegularUser("user@test.com");

  EXPECT_TRUE(browser_util::IsLacrosEnabled(Channel::UNKNOWN));
  EXPECT_TRUE(browser_util::IsLacrosEnabled(Channel::CANARY));
  EXPECT_TRUE(browser_util::IsLacrosEnabled(Channel::DEV));
  EXPECT_TRUE(browser_util::IsLacrosEnabled(Channel::BETA));
  EXPECT_FALSE(browser_util::IsLacrosEnabled(Channel::STABLE));
}

TEST_F(LacrosUtilTest, ManagedAccountLacrosEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(chromeos::features::kLacrosSupport);
  AddRegularUser("user@managedchrome.com");
  testing_profile_.GetProfilePolicyConnector()->OverrideIsManagedForTesting(
      true);
  g_browser_process->local_state()->SetBoolean(prefs::kLacrosAllowed, true);

  EXPECT_TRUE(browser_util::IsLacrosEnabled(Channel::CANARY));
}

TEST_F(LacrosUtilTest, ManagedAccountLacrosDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(chromeos::features::kLacrosSupport);
  AddRegularUser("user@managedchrome.com");
  testing_profile_.GetProfilePolicyConnector()->OverrideIsManagedForTesting(
      true);
  g_browser_process->local_state()->SetBoolean(prefs::kLacrosAllowed, false);

  EXPECT_FALSE(browser_util::IsLacrosEnabled(Channel::CANARY));
}

TEST_F(LacrosUtilTest, BlockedForChildUser) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(chromeos::features::kLacrosSupport);
  AccountId account_id = AccountId::FromUserEmail("user@test.com");
  const User* user = fake_user_manager_->AddChildUser(account_id);
  fake_user_manager_->UserLoggedIn(account_id, user->username_hash(),
                                   /*browser_restart=*/false,
                                   /*is_child=*/true);
  EXPECT_FALSE(browser_util::IsLacrosEnabled(Channel::UNKNOWN));
}

TEST_F(LacrosUtilTest, GetInterfaceVersions) {
  base::flat_map<base::Token, uint32_t> versions =
      browser_util::GetInterfaceVersions();

  // Check that a known interface with version > 0 is present and has non-zero
  // version.
  EXPECT_GT(versions[mojom::KeystoreService::Uuid_], 0);

  // Check that the empty token is not present.
  base::Token token;
  auto it = versions.find(token);
  EXPECT_EQ(it, versions.end());
}

}  // namespace crosapi
