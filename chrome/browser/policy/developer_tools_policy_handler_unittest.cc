// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/developer_tools_policy_handler.h"

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/policy/core/browser/configuration_policy_pref_store_test.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)

#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"

#endif

namespace policy {

using Availability = DeveloperToolsPolicyHandler::Availability;

class DeveloperToolsPolicyHandlerTest
    : public ConfigurationPolicyPrefStoreTest {
 public:
  DeveloperToolsPolicyHandlerTest() {
    handler_list_.AddHandler(std::make_unique<DeveloperToolsPolicyHandler>());
  }
};

TEST_F(DeveloperToolsPolicyHandlerTest, NewPolicyOverridesLegacyPolicy) {
  EXPECT_FALSE(store_->GetValue(prefs::kDevToolsAvailability, nullptr));

  PolicyMap policy;
  policy.Set(
      key::kDeveloperToolsAvailability, POLICY_LEVEL_MANDATORY,
      POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
      base::Value(0 /*DeveloperToolsDisallowedForForceInstalledExtensions*/),
      nullptr);
  policy.Set(key::kDeveloperToolsDisabled, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(true),
             nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* value = nullptr;
  ASSERT_TRUE(store_->GetValue(prefs::kDevToolsAvailability, &value));
  EXPECT_EQ(
      static_cast<int>(Availability::kDisallowedForForceInstalledExtensions),
      value->GetInt());

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // No force-disabling of developer mode on extensions UI.
  EXPECT_FALSE(store_->GetValue(prefs::kExtensionsUIDeveloperMode, nullptr));
#endif
}

TEST_F(DeveloperToolsPolicyHandlerTest, LegacyPolicyAppliesIfNewPolicyInvalid) {
  EXPECT_FALSE(store_->GetValue(prefs::kDevToolsAvailability, nullptr));

  PolicyMap policy;
  policy.Set(key::kDeveloperToolsAvailability, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
             base::Value(5 /*out of range*/), nullptr);
  policy.Set(key::kDeveloperToolsDisabled, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(true),
             nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* value = nullptr;
  ASSERT_TRUE(store_->GetValue(prefs::kDevToolsAvailability, &value));
  EXPECT_EQ(static_cast<int>(Availability::kDisallowed), value->GetInt());

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Developer mode on extensions UI is also disabled.
  const base::Value* extensions_ui_dev_mode_value = nullptr;
  ASSERT_TRUE(store_->GetValue(prefs::kExtensionsUIDeveloperMode,
                               &extensions_ui_dev_mode_value));
  EXPECT_FALSE(extensions_ui_dev_mode_value->GetBool());
#endif
}

TEST_F(DeveloperToolsPolicyHandlerTest, NewPolicyAppliesIfLegacyPolicyInvalid) {
  EXPECT_FALSE(store_->GetValue(prefs::kDevToolsAvailability, nullptr));

  PolicyMap policy;
  policy.Set(key::kDeveloperToolsAvailability, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
             base::Value(1 /*kAllowed*/), nullptr);
  policy.Set(key::kDeveloperToolsDisabled, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
             base::Value(4 /*wrong type*/), nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* value = nullptr;
  ASSERT_TRUE(store_->GetValue(prefs::kDevToolsAvailability, &value));
  EXPECT_EQ(static_cast<int>(Availability::kAllowed), value->GetInt());
}

TEST_F(DeveloperToolsPolicyHandlerTest, DisallowedForForceInstalledExtensions) {
  EXPECT_FALSE(store_->GetValue(prefs::kDevToolsAvailability, nullptr));

  PolicyMap policy;
  policy.Set(
      key::kDeveloperToolsAvailability, POLICY_LEVEL_MANDATORY,
      POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
      base::Value(0 /*DeveloperToolsDisallowedForForceInstalledExtensions*/),
      nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* value = nullptr;
  ASSERT_TRUE(store_->GetValue(prefs::kDevToolsAvailability, &value));
  EXPECT_EQ(
      static_cast<int>(Availability::kDisallowedForForceInstalledExtensions),
      value->GetInt());

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // No force-disabling of developer mode on extensions UI.
  EXPECT_FALSE(store_->GetValue(prefs::kExtensionsUIDeveloperMode, nullptr));
#endif
}

TEST_F(DeveloperToolsPolicyHandlerTest, Allowed) {
  EXPECT_FALSE(store_->GetValue(prefs::kDevToolsAvailability, nullptr));

  PolicyMap policy;
  policy.Set(key::kDeveloperToolsAvailability, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
             base::Value(1 /*DeveloperToolsAllowed*/), nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* value = nullptr;
  ASSERT_TRUE(store_->GetValue(prefs::kDevToolsAvailability, &value));
  EXPECT_EQ(static_cast<int>(Availability::kAllowed), value->GetInt());

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // No force-disabling of developer mode on extensions UI.
  EXPECT_FALSE(store_->GetValue(prefs::kExtensionsUIDeveloperMode, nullptr));
#endif
}

TEST_F(DeveloperToolsPolicyHandlerTest, Disallowed) {
  EXPECT_FALSE(store_->GetValue(prefs::kDevToolsAvailability, nullptr));

  PolicyMap policy;
  policy.Set(key::kDeveloperToolsAvailability, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
             base::Value(2 /*Disallowed*/), nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* value = nullptr;
  ASSERT_TRUE(store_->GetValue(prefs::kDevToolsAvailability, &value));
  EXPECT_EQ(static_cast<int>(Availability::kDisallowed), value->GetInt());

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Developer mode on extensions UI is also disabled.
  const base::Value* extensions_ui_dev_mode_value = nullptr;
  ASSERT_TRUE(store_->GetValue(prefs::kExtensionsUIDeveloperMode,
                               &extensions_ui_dev_mode_value));
  EXPECT_FALSE(extensions_ui_dev_mode_value->GetBool());
#endif
}

TEST_F(DeveloperToolsPolicyHandlerTest, InvalidValue) {
  EXPECT_FALSE(store_->GetValue(prefs::kDevToolsAvailability, nullptr));

  PolicyMap policy;
  policy.Set(key::kDeveloperToolsAvailability, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
             base::Value(5 /*out of range*/), nullptr);
  UpdateProviderPolicy(policy);
  EXPECT_FALSE(store_->GetValue(prefs::kDevToolsAvailability, nullptr));

#if BUILDFLAG(ENABLE_EXTENSIONS)
  EXPECT_FALSE(store_->GetValue(prefs::kExtensionsUIDeveloperMode, nullptr));
#endif
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

struct TestParam {
  Availability primary_profile_availability;
  Availability secondary_profile_availability;
  Availability expected_result;
};

// Tests static function of `DeveloperToolsPolicyHandler` requiring primary and
// secondary profiles.
class DeveloperToolsPolicyHandlerWithProfileTest
    : public testing::TestWithParam<TestParam> {
 public:
  DeveloperToolsPolicyHandlerWithProfileTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());

    auto user_manager = std::make_unique<ash::FakeChromeUserManager>();
    {
      constexpr char kPrimaryProfileName[] = "primary_profile";
      const AccountId account_id(AccountId::FromUserEmail(kPrimaryProfileName));
      primary_profile_ = profile_manager_.CreateTestingProfile(
          kPrimaryProfileName, /* is_main_profile= */ true);

      user_manager->AddUserWithAffiliationAndTypeAndProfile(
          account_id, /* is_affiliated= */ true,
          user_manager::UserType::kRegular, primary_profile_);
      user_manager->LoginUser(account_id);
    }
    {
      constexpr char kSecondaryProfileName[] = "secondary_profile";
      const AccountId account_id(
          AccountId::FromUserEmail(kSecondaryProfileName));
      secondary_profile_ =
          profile_manager_.CreateTestingProfile(kSecondaryProfileName);

      user_manager->AddUserWithAffiliationAndTypeAndProfile(
          account_id, /* is_affiliated= */ true,
          user_manager::UserType::kRegular, secondary_profile_);
    }
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));

    EXPECT_TRUE(ash::ProfileHelper::IsPrimaryProfile(primary_profile_));
    EXPECT_FALSE(ash::ProfileHelper::IsPrimaryProfile(secondary_profile_));
  }

  void UpdatePrimaryProfileAvailability() {
    primary_profile_->GetTestingPrefService()->SetManagedPref(
        prefs::kDevToolsAvailability,
        base::Value(static_cast<int>(GetParam().primary_profile_availability)));
  }

  void UpdateSecondaryProfileAvailability() {
    secondary_profile_->GetTestingPrefService()->SetManagedPref(
        prefs::kDevToolsAvailability,
        base::Value(
            static_cast<int>(GetParam().secondary_profile_availability)));
  }

  Availability expected_result() const { return GetParam().expected_result; }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  raw_ptr<TestingProfile> primary_profile_;
  raw_ptr<TestingProfile> secondary_profile_;
};

TEST_F(DeveloperToolsPolicyHandlerWithProfileTest,
       GetEffectiveAvailabilityForced) {
  EXPECT_EQ(
      Availability::kDisallowedForForceInstalledExtensions,
      DeveloperToolsPolicyHandler::GetEffectiveAvailability(primary_profile_));

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(switches::kForceDevToolsAvailable);

  EXPECT_EQ(
      Availability::kAllowed,
      DeveloperToolsPolicyHandler::GetEffectiveAvailability(primary_profile_));
}

TEST_P(DeveloperToolsPolicyHandlerWithProfileTest,
       GetEffectiveAvailabilityRestrictive) {
  UpdatePrimaryProfileAvailability();
  UpdateSecondaryProfileAvailability();
  EXPECT_EQ(expected_result(),
            DeveloperToolsPolicyHandler::GetEffectiveAvailability(
                secondary_profile_));
}

INSTANTIATE_TEST_SUITE_P(
    DeveloperToolsPolicyHandlerTest,
    DeveloperToolsPolicyHandlerWithProfileTest,
    ::testing::Values(
        TestParam(
            /* primary_profile_availability= */ Availability::kAllowed,
            /* secondary_profile_availability= */ Availability::kAllowed,
            /* expected_result= */ Availability::kAllowed),
        TestParam(
            /* primary_profile_availability= */ Availability::kDisallowed,
            /* secondary_profile_availability= */ Availability::kAllowed,
            /* expected_result= */ Availability::kDisallowed),
        TestParam(
            /* primary_profile_availability= */ Availability::kAllowed,
            /* secondary_profile_availability= */ Availability::kDisallowed,
            /* expected_result= */ Availability::kDisallowed),
        TestParam(
            /* primary_profile_availability= */ Availability::kAllowed,
            /* secondary_profile_availability= */
            Availability::kDisallowedForForceInstalledExtensions,
            /* expected_result= */
            Availability::kDisallowedForForceInstalledExtensions),
        TestParam(
            /* primary_profile_availability= */ Availability::
                kDisallowedForForceInstalledExtensions,
            /* secondary_profile_availability= */ Availability::kAllowed,
            /* expected_result= */
            Availability::kDisallowedForForceInstalledExtensions),
        TestParam(
            /* primary_profile_availability= */ Availability::kDisallowed,
            /* secondary_profile_availability= */
            Availability::kDisallowedForForceInstalledExtensions,
            /* expected_result= */ Availability::kDisallowed),
        TestParam(
            /* primary_profile_availability= */ Availability::
                kDisallowedForForceInstalledExtensions,
            /* secondary_profile_availability= */ Availability::kDisallowed,
            /* expected_result= */ Availability::kDisallowed),
        TestParam(
            /* primary_profile_availability= */ Availability::kDisallowed,
            /* secondary_profile_availability= */ Availability::kDisallowed,
            /* expected_result= */ Availability::kDisallowed)));
#endif

}  // namespace policy
