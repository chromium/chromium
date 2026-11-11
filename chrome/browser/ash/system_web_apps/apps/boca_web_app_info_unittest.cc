// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/boca_web_app_info.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/webui/boca_ui/url_constants.h"
#include "base/containers/flat_set.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/test/chrome_user_session_test_environment_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/policy/device_local_account/device_local_account_type.h"
#include "components/account_id/account_id.h"
#include "components/account_id/account_id_literal.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "components/tabs/public/tab_context_menu_command.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace {

constexpr auto kAccountId =
    AccountId::Literal::FromUserEmailGaiaId("user@example.com",
                                            GaiaId::Literal("1234567890"));

class BocaSystemAppDelegateTest : public testing::Test {
 protected:
  void SetUp() override {
    auto* browser_process = TestingBrowserProcess::GetGlobal();
    user_session_test_environment_ = std::make_unique<
        ash::test::UserSessionTestEnvironment>(
        browser_process->local_state(),
        std::make_unique<ash::test::ChromeUserSessionTestEnvironmentDelegate>(
            browser_process));
    const user_manager::User* user = AddUser();
    ASSERT_TRUE(user);
    user_session_test_environment_->LogIn(user->GetAccountId());
    profile_ = Profile::FromBrowserContext(
        ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
            user->GetAccountId()));
    ASSERT_TRUE(profile_);
    delegate_ = std::make_unique<BocaSystemAppDelegate>(profile_);
  }

  void TearDown() override {
    delegate_.reset();
    profile_ = nullptr;
    user_session_test_environment_.reset();
  }

  // Adds the user the test logs in. A regular user by default.
  virtual user_manager::User* AddUser() {
    return user_session_test_environment_->AddRegularUser(kAccountId);
  }

  // Marks the logged-in user as affiliated with the device.
  void SetUserAffiliated() {
    user_manager::UserManager::Get()->SetUserPolicyStatus(
        kAccountId, /*is_managed=*/true, /*is_affiliated=*/true);
  }

  const BocaSystemAppDelegate* delegate() const { return delegate_.get(); }
  ash::test::UserSessionTestEnvironment* user_session_test_environment() {
    return user_session_test_environment_.get();
  }
  base::test::ScopedFeatureList* scoped_feature_list() {
    return &scoped_feature_list_;
  }
  Profile* profile() { return profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<ash::test::UserSessionTestEnvironment>
      user_session_test_environment_;
  std::unique_ptr<BocaSystemAppDelegate> delegate_;
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<Profile> profile_ = nullptr;
};

TEST_F(BocaSystemAppDelegateTest, AppShowFromSearchAndShelfByDefault) {
  EXPECT_TRUE(delegate()->ShouldShowInSearchAndShelf());
  EXPECT_FALSE(delegate()->IsAppEnabled());
}

TEST_F(BocaSystemAppDelegateTest, AppShowFromLauncherByDefault) {
  EXPECT_TRUE(delegate()->ShouldShowInLauncher());
  EXPECT_FALSE(delegate()->IsAppEnabled());
}

TEST_F(BocaSystemAppDelegateTest, AppDisabledByKillSwitch) {
  scoped_feature_list()->InitAndDisableFeature(ash::features::kBocaUber);
  EXPECT_FALSE(delegate()->IsAppEnabled());
}

TEST_F(BocaSystemAppDelegateTest, AppEnabledWhenFeatureEnabled) {
  scoped_feature_list()->InitAndEnableFeature(ash::features::kBoca);
  EXPECT_TRUE(delegate()->IsAppEnabled());
}

TEST_F(BocaSystemAppDelegateTest, AppDisabledForUnAffliatedUser) {
  EXPECT_FALSE(delegate()->IsAppEnabled());
}

TEST_F(BocaSystemAppDelegateTest, AppDisabledFromPref) {
  SetUserAffiliated();
  profile()->GetPrefs()->SetString(
      ash::prefs::kClassManagementToolsAvailabilitySetting, "disabled");
  EXPECT_FALSE(delegate()->IsAppEnabled());
}

TEST_F(BocaSystemAppDelegateTest, BocaDisabledByDefaultFromPref) {
  SetUserAffiliated();
  profile()->GetPrefs()->SetString(
      ash::prefs::kClassManagementToolsAvailabilitySetting, "");
  EXPECT_FALSE(delegate()->IsAppEnabled());
}

TEST_F(BocaSystemAppDelegateTest, AppEnabledFromPref) {
  SetUserAffiliated();
  profile()->GetPrefs()->SetString(
      ash::prefs::kClassManagementToolsAvailabilitySetting, "teacher");
  EXPECT_TRUE(delegate()->IsAppEnabled());
}

TEST_F(BocaSystemAppDelegateTest, OverrideURLScopeChecks) {
  SetUserAffiliated();
  EXPECT_TRUE(delegate()->IsUrlInSystemAppScope(GURL()));
}

class BocaSystemAppProviderDelegateTest : public BocaSystemAppDelegateTest {
 public:
  BocaSystemAppProviderDelegateTest() {
    scoped_feature_list()->InitWithFeatures(
        /*enabled_features=*/{ash::features::kBoca},
        /*disabled_features=*/{ash::features::kBocaConsumer});
  }
};

TEST_F(BocaSystemAppProviderDelegateTest, MissingTabStrip) {
  EXPECT_FALSE(delegate()->ShouldHaveTabStrip());
}

TEST_F(BocaSystemAppProviderDelegateTest, DoNotOverrideURLScopeChecks) {
  EXPECT_FALSE(delegate()->IsUrlInSystemAppScope(GURL()));
}

TEST_F(BocaSystemAppProviderDelegateTest, AllowResize) {
  EXPECT_TRUE(delegate()->ShouldAllowResize());
}

TEST_F(BocaSystemAppProviderDelegateTest, AllowMaximize) {
  EXPECT_TRUE(delegate()->ShouldAllowMaximize());
}

TEST_F(BocaSystemAppProviderDelegateTest, HasMinimalSize) {
  EXPECT_EQ(gfx::Size(400, 400), delegate()->GetMinimumWindowSize());
}

TEST_F(BocaSystemAppProviderDelegateTest, UsesDefaultTabMenuModel) {
  EXPECT_FALSE(delegate()->GetAllowedTabMenuCommands().has_value());
}

class BocaSystemAppConsumerDelegateTest : public BocaSystemAppDelegateTest {
 public:
  BocaSystemAppConsumerDelegateTest() {
    scoped_feature_list()->InitWithFeatures(
        /*enabled_features=*/{ash::features::kBoca,
                              ash::features::kBocaConsumer},
        /*disabled_features=*/{});
  }
};

TEST_F(BocaSystemAppConsumerDelegateTest, ShouldHaveTabStrip) {
  EXPECT_TRUE(delegate()->ShouldHaveTabStrip());
}

TEST_F(BocaSystemAppConsumerDelegateTest, OverrideURLScopeChecks) {
  EXPECT_TRUE(delegate()->IsUrlInSystemAppScope(GURL()));
}

TEST_F(BocaSystemAppConsumerDelegateTest, DisallowResize) {
  EXPECT_FALSE(delegate()->ShouldAllowResize());
}

TEST_F(BocaSystemAppConsumerDelegateTest, DisallowMaximize) {
  EXPECT_FALSE(delegate()->ShouldAllowMaximize());
}

TEST_F(BocaSystemAppConsumerDelegateTest, PinHomeTab) {
  EXPECT_TRUE(delegate()->ShouldPinTab(
      GURL(ash::boca::kChromeBocaAppUntrustedIndexURL)));
}

TEST_F(BocaSystemAppConsumerDelegateTest, HideNewTabButton) {
  EXPECT_TRUE(delegate()->ShouldHideNewTabButton());
}

TEST_F(BocaSystemAppConsumerDelegateTest, HasMinimalSize) {
  EXPECT_EQ(gfx::Size(500, 500), delegate()->GetMinimumWindowSize());
}

TEST_F(BocaSystemAppConsumerDelegateTest, AllowedTabMenuCommands) {
  auto commands = delegate()->GetAllowedTabMenuCommands();
  ASSERT_TRUE(commands.has_value());
  EXPECT_EQ((base::flat_set<tabs::TabContextMenuCommand>{tabs::CommandReload,
                                                         tabs::CommandGoBack}),
            *commands);
}

// Logs in a managed guest session instead of a regular user.
class BocaSystemAppDelegatePublicAccountTest
    : public BocaSystemAppDelegateTest {
 protected:
  user_manager::User* AddUser() override {
    return user_session_test_environment()->AddPublicAccountUser(
        policy::GenerateDeviceLocalAccountUserId(
            "public-account", policy::DeviceLocalAccountType::kPublicSession));
  }
};

TEST_F(BocaSystemAppDelegatePublicAccountTest,
       AppDisabledForDeviceLocalAccount) {
  profile()->GetPrefs()->SetString(
      ash::prefs::kClassManagementToolsAvailabilitySetting, "teacher");
  EXPECT_FALSE(delegate()->IsAppEnabled());
}

}  // namespace
