// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_util.h"

#include "ash/constants/ash_features.h"
#include "base/json/json_reader.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/version_info/channel.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using user_manager::User;
using version_info::Channel;

namespace crosapi {

class BrowserUtilTest : public testing::Test {
 public:
  BrowserUtilTest() : local_state_(TestingBrowserProcess::GetGlobal()) {}
  ~BrowserUtilTest() override = default;

  void SetUp() override {
    fake_user_manager_ = new ash::FakeChromeUserManager;
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
  ash::FakeChromeUserManager* fake_user_manager_ = nullptr;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;

  ScopedTestingLocalState local_state_;
};

TEST_F(BrowserUtilTest, LacrosEnabledByFlag) {
  AddRegularUser("user@test.com");

  // Lacros is disabled because the feature isn't enabled by default.
  EXPECT_FALSE(browser_util::IsLacrosEnabled());

  // Enabling the flag enables Lacros.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(chromeos::features::kLacrosSupport);
  EXPECT_TRUE(browser_util::IsLacrosEnabled());
}

TEST_F(BrowserUtilTest, LacrosEnabledForChannels) {
  AddRegularUser("user@test.com");

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(chromeos::features::kLacrosSupport);
    EXPECT_TRUE(browser_util::IsLacrosEnabled(Channel::UNKNOWN));
    EXPECT_TRUE(browser_util::IsLacrosEnabled(Channel::CANARY));
    EXPECT_TRUE(browser_util::IsLacrosEnabled(Channel::DEV));
    EXPECT_TRUE(browser_util::IsLacrosEnabled(Channel::BETA));
    EXPECT_TRUE(browser_util::IsLacrosEnabled(Channel::STABLE));
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures({chromeos::features::kLacrosSupport},
                                  {browser_util::kLacrosAllowOnStableChannel});
    EXPECT_TRUE(browser_util::IsLacrosEnabled(Channel::UNKNOWN));
    EXPECT_TRUE(browser_util::IsLacrosEnabled(Channel::CANARY));
    EXPECT_TRUE(browser_util::IsLacrosEnabled(Channel::DEV));
    EXPECT_TRUE(browser_util::IsLacrosEnabled(Channel::BETA));
    EXPECT_FALSE(browser_util::IsLacrosEnabled(Channel::STABLE));
  }
}

TEST_F(BrowserUtilTest, ManagedAccountLacrosEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(chromeos::features::kLacrosSupport);
  AddRegularUser("user@managedchrome.com");
  testing_profile_.GetProfilePolicyConnector()->OverrideIsManagedForTesting(
      true);

  {
    g_browser_process->local_state()->SetBoolean(prefs::kLacrosAllowed, true);
    EXPECT_TRUE(browser_util::IsLacrosEnabled(Channel::CANARY));
  }

  {
    g_browser_process->local_state()->SetBoolean(prefs::kLacrosAllowed, false);

    g_browser_process->local_state()->SetInteger(
        prefs::kLacrosLaunchSwitch,
        static_cast<int>(browser_util::LacrosLaunchSwitch::kSideBySide));
    EXPECT_TRUE(browser_util::IsLacrosEnabled(Channel::CANARY));

    g_browser_process->local_state()->SetInteger(
        prefs::kLacrosLaunchSwitch,
        static_cast<int>(browser_util::LacrosLaunchSwitch::kLacrosPrimary));
    EXPECT_TRUE(browser_util::IsLacrosEnabled(Channel::CANARY));

    g_browser_process->local_state()->SetInteger(
        prefs::kLacrosLaunchSwitch,
        static_cast<int>(browser_util::LacrosLaunchSwitch::kLacrosOnly));
    EXPECT_TRUE(browser_util::IsLacrosEnabled(Channel::CANARY));
  }
}

TEST_F(BrowserUtilTest, ManagedAccountLacrosDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(chromeos::features::kLacrosSupport);
  AddRegularUser("user@managedchrome.com");
  testing_profile_.GetProfilePolicyConnector()->OverrideIsManagedForTesting(
      true);

  {
    g_browser_process->local_state()->SetBoolean(prefs::kLacrosAllowed, false);
    EXPECT_FALSE(browser_util::IsLacrosEnabled(Channel::CANARY));
  }

  {
    g_browser_process->local_state()->SetBoolean(prefs::kLacrosAllowed, true);
    g_browser_process->local_state()->SetInteger(
        prefs::kLacrosLaunchSwitch,
        static_cast<int>(browser_util::LacrosLaunchSwitch::kLacrosDisallowed));
    EXPECT_FALSE(browser_util::IsLacrosEnabled(Channel::CANARY));
  }
}

TEST_F(BrowserUtilTest, BlockedForChildUser) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(chromeos::features::kLacrosSupport);
  AccountId account_id = AccountId::FromUserEmail("user@test.com");
  const User* user = fake_user_manager_->AddChildUser(account_id);
  fake_user_manager_->UserLoggedIn(account_id, user->username_hash(),
                                   /*browser_restart=*/false,
                                   /*is_child=*/true);
  EXPECT_FALSE(browser_util::IsLacrosEnabled(Channel::UNKNOWN));
}

TEST_F(BrowserUtilTest, AshWebBrowserEnabled) {
  base::test::ScopedFeatureList feature_list;
  AddRegularUser("user@managedchrome.com");
  testing_profile_.GetProfilePolicyConnector()->OverrideIsManagedForTesting(
      true);

  // Lacros is not allowed.
  {
    g_browser_process->local_state()->SetInteger(
        prefs::kLacrosLaunchSwitch,
        static_cast<int>(browser_util::LacrosLaunchSwitch::kLacrosDisallowed));

    EXPECT_FALSE(browser_util::IsLacrosAllowedToBeEnabled(Channel::CANARY));
    EXPECT_FALSE(browser_util::IsLacrosEnabled(Channel::CANARY));
    EXPECT_TRUE(browser_util::IsAshWebBrowserEnabled(Channel::CANARY));
  }

  // Lacros is allowed but not enabled.
  {
    g_browser_process->local_state()->SetInteger(
        prefs::kLacrosLaunchSwitch,
        static_cast<int>(browser_util::LacrosLaunchSwitch::kUserChoice));

    EXPECT_TRUE(browser_util::IsLacrosAllowedToBeEnabled(Channel::CANARY));
    EXPECT_FALSE(browser_util::IsLacrosEnabled(Channel::CANARY));
    EXPECT_TRUE(browser_util::IsAshWebBrowserEnabled(Channel::CANARY));
  }

  // Lacros is allowed and enabled by flag.
  {
    g_browser_process->local_state()->SetInteger(
        prefs::kLacrosLaunchSwitch,
        static_cast<int>(browser_util::LacrosLaunchSwitch::kUserChoice));
    feature_list.InitAndEnableFeature(chromeos::features::kLacrosSupport);

    EXPECT_TRUE(browser_util::IsLacrosAllowedToBeEnabled(Channel::CANARY));
    EXPECT_TRUE(browser_util::IsLacrosEnabled(Channel::CANARY));
    EXPECT_TRUE(browser_util::IsAshWebBrowserEnabled(Channel::CANARY));
  }

  // Lacros is allowed and enabled by policy.
  {
    g_browser_process->local_state()->SetInteger(
        prefs::kLacrosLaunchSwitch,
        static_cast<int>(browser_util::LacrosLaunchSwitch::kSideBySide));

    EXPECT_TRUE(browser_util::IsLacrosAllowedToBeEnabled(Channel::CANARY));
    EXPECT_TRUE(browser_util::IsLacrosEnabled(Channel::CANARY));
    EXPECT_TRUE(browser_util::IsAshWebBrowserEnabled(Channel::CANARY));

    g_browser_process->local_state()->SetInteger(
        prefs::kLacrosLaunchSwitch,
        static_cast<int>(browser_util::LacrosLaunchSwitch::kLacrosPrimary));

    EXPECT_TRUE(browser_util::IsLacrosAllowedToBeEnabled(Channel::CANARY));
    EXPECT_TRUE(browser_util::IsLacrosEnabled(Channel::CANARY));
    EXPECT_TRUE(browser_util::IsAshWebBrowserEnabled(Channel::CANARY));
  }
}

TEST_F(BrowserUtilTest, IsAshWebBrowserDisabled) {
  base::test::ScopedFeatureList feature_list;
  AddRegularUser("user@managedchrome.com");
  testing_profile_.GetProfilePolicyConnector()->OverrideIsManagedForTesting(
      true);

  g_browser_process->local_state()->SetInteger(
      prefs::kLacrosLaunchSwitch,
      static_cast<int>(browser_util::LacrosLaunchSwitch::kLacrosOnly));

  // Lacros is allowed and enabled and is the only browser by policy.

  EXPECT_TRUE(browser_util::IsLacrosEnabled(Channel::UNKNOWN));
  EXPECT_FALSE(browser_util::IsAshWebBrowserEnabled(Channel::UNKNOWN));

  EXPECT_TRUE(browser_util::IsLacrosEnabled(Channel::CANARY));
  EXPECT_FALSE(browser_util::IsAshWebBrowserEnabled(Channel::CANARY));

  EXPECT_TRUE(browser_util::IsLacrosEnabled(Channel::DEV));
  EXPECT_FALSE(browser_util::IsAshWebBrowserEnabled(Channel::DEV));

  EXPECT_TRUE(browser_util::IsLacrosEnabled(Channel::BETA));
  EXPECT_FALSE(browser_util::IsAshWebBrowserEnabled(Channel::BETA));

  EXPECT_TRUE(browser_util::IsLacrosEnabled(Channel::STABLE));
  EXPECT_FALSE(browser_util::IsAshWebBrowserEnabled(Channel::STABLE));
}

TEST_F(BrowserUtilTest, LacrosPrimaryBrowserByFlags) {
  AddRegularUser("user@test.com");

  EXPECT_FALSE(browser_util::IsLacrosPrimaryBrowser());

  // Just enabling LacrosPrimary feature is not enough.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(chromeos::features::kLacrosPrimary);
    EXPECT_FALSE(browser_util::IsLacrosPrimaryBrowser());
  }

  // Both LacrosPrimary and LacrosSupport are needed.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures({chromeos::features::kLacrosPrimary,
                                   chromeos::features::kLacrosSupport},
                                  {});
    EXPECT_TRUE(browser_util::IsLacrosPrimaryBrowser());
  }
}

TEST_F(BrowserUtilTest, LacrosPrimaryBrowserForChannels) {
  AddRegularUser("user@test.com");

  // Currently, only developer build can use Lacros as a primary
  // web browser.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {chromeos::features::kLacrosPrimary, chromeos::features::kLacrosSupport},
      {});
  EXPECT_TRUE(browser_util::IsLacrosPrimaryBrowser(Channel::UNKNOWN));
  EXPECT_FALSE(browser_util::IsLacrosPrimaryBrowser(Channel::CANARY));
  EXPECT_FALSE(browser_util::IsLacrosPrimaryBrowser(Channel::DEV));
  EXPECT_FALSE(browser_util::IsLacrosPrimaryBrowser(Channel::BETA));
  EXPECT_FALSE(browser_util::IsLacrosPrimaryBrowser(Channel::STABLE));
}

TEST_F(BrowserUtilTest, LacrosPrimaryBrowserAllowedForChannels) {
  AddRegularUser("user@test.com");

  EXPECT_TRUE(browser_util::IsLacrosPrimaryBrowserAllowed(Channel::UNKNOWN));
  EXPECT_FALSE(browser_util::IsLacrosPrimaryBrowserAllowed(Channel::CANARY));
  EXPECT_FALSE(browser_util::IsLacrosPrimaryBrowserAllowed(Channel::DEV));
  EXPECT_FALSE(browser_util::IsLacrosPrimaryBrowserAllowed(Channel::BETA));
  EXPECT_FALSE(browser_util::IsLacrosPrimaryBrowserAllowed(Channel::STABLE));
}

TEST_F(BrowserUtilTest, ManagedAccountLacrosPrimary) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(chromeos::features::kLacrosSupport);
  AddRegularUser("user@managedchrome.com");
  testing_profile_.GetProfilePolicyConnector()->OverrideIsManagedForTesting(
      true);

  {
    g_browser_process->local_state()->SetInteger(
        prefs::kLacrosLaunchSwitch,
        static_cast<int>(browser_util::LacrosLaunchSwitch::kLacrosDisallowed));
    EXPECT_FALSE(browser_util::IsLacrosPrimaryBrowserAllowed(Channel::UNKNOWN));
    EXPECT_FALSE(browser_util::IsLacrosPrimaryBrowser(Channel::UNKNOWN));
  }

  {
    g_browser_process->local_state()->SetInteger(
        prefs::kLacrosLaunchSwitch,
        static_cast<int>(browser_util::LacrosLaunchSwitch::kSideBySide));
    EXPECT_TRUE(browser_util::IsLacrosPrimaryBrowserAllowed(Channel::UNKNOWN));
    EXPECT_FALSE(browser_util::IsLacrosPrimaryBrowser(Channel::UNKNOWN));
  }

  {
    g_browser_process->local_state()->SetInteger(
        prefs::kLacrosLaunchSwitch,
        static_cast<int>(browser_util::LacrosLaunchSwitch::kLacrosPrimary));
    EXPECT_TRUE(browser_util::IsLacrosPrimaryBrowserAllowed(Channel::UNKNOWN));
    EXPECT_TRUE(browser_util::IsLacrosPrimaryBrowser(Channel::UNKNOWN));
  }

  {
    g_browser_process->local_state()->SetInteger(
        prefs::kLacrosLaunchSwitch,
        static_cast<int>(browser_util::LacrosLaunchSwitch::kLacrosOnly));
    EXPECT_TRUE(browser_util::IsLacrosPrimaryBrowserAllowed(Channel::UNKNOWN));
    EXPECT_TRUE(browser_util::IsLacrosPrimaryBrowser(Channel::UNKNOWN));
  }
}

TEST_F(BrowserUtilTest, GetInterfaceVersions) {
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

TEST_F(BrowserUtilTest, MetadataMissing) {
  EXPECT_FALSE(browser_util::DoesMetadataSupportNewAccountManager(nullptr));
}

TEST_F(BrowserUtilTest, MetadataMissingVersion) {
  std::string json_string = R"###(
   {
     "content": {
     },
     "metadata_version": 1
   }
  )###";
  base::Optional<base::Value> value = base::JSONReader::Read(json_string);
  EXPECT_FALSE(
      browser_util::DoesMetadataSupportNewAccountManager(&value.value()));
}

TEST_F(BrowserUtilTest, MetadataVersionBadFormat) {
  std::string json_string = R"###(
   {
     "content": {
       "version": "91.0.4469"
     },
     "metadata_version": 1
   }
  )###";
  base::Optional<base::Value> value = base::JSONReader::Read(json_string);
  EXPECT_FALSE(
      browser_util::DoesMetadataSupportNewAccountManager(&value.value()));
}

TEST_F(BrowserUtilTest, MetadataOldVersion) {
  std::string json_string = R"###(
   {
     "content": {
       "version": "91.0.4469.5"
     },
     "metadata_version": 1
   }
  )###";
  base::Optional<base::Value> value = base::JSONReader::Read(json_string);
  EXPECT_FALSE(
      browser_util::DoesMetadataSupportNewAccountManager(&value.value()));
}

TEST_F(BrowserUtilTest, MetadataNewVersion) {
  std::string json_string = R"###(
   {
     "content": {
       "version": "9999.0.4469.5"
     },
     "metadata_version": 1
   }
  )###";
  base::Optional<base::Value> value = base::JSONReader::Read(json_string);
  EXPECT_TRUE(
      browser_util::DoesMetadataSupportNewAccountManager(&value.value()));
}

}  // namespace crosapi
