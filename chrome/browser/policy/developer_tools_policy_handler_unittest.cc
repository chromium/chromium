// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/developer_tools_policy_handler.h"

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/policy/core/browser/configuration_policy_pref_store_test.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/login/users/scoped_account_id_annotator.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id_literal.h"
#include "components/session_manager/core/fake_session_manager_delegate.h"
#include "components/session_manager/core/session_manager.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/fake_user_manager_delegate.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/test_helper.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_impl.h"
#include "content/public/test/browser_task_environment.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace policy {

#if BUILDFLAG(IS_CHROMEOS)
namespace {
constexpr char kPrimaryProfileName[] = "primary_profile@test";
constexpr auto kPrimaryUserAccountId =
    AccountId::Literal::FromUserEmailGaiaId(kPrimaryProfileName,
                                            GaiaId::Literal("123"));

constexpr char kSecondaryProfileName[] = "secondary_profile@test";
constexpr auto kSecondaryUserAccountId =
    AccountId::Literal::FromUserEmailGaiaId(kSecondaryProfileName,
                                            GaiaId::Literal("abc"));
}  // namespace
#endif  // BUILDFLAG(IS_CHROMEOS)

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

#if BUILDFLAG(IS_CHROMEOS)

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

    session_manager_ = std::make_unique<session_manager::SessionManager>(
        std::make_unique<session_manager::FakeSessionManagerDelegate>());

    user_manager_.Reset(std::make_unique<user_manager::UserManagerImpl>(
        std::make_unique<user_manager::FakeUserManagerDelegate>(),
        TestingBrowserProcess::GetGlobal()->local_state()));
    session_manager::SessionManager::Get()->OnUserManagerCreated(
        user_manager_.Get());

    {
      ASSERT_TRUE(user_manager::TestHelper(user_manager_.Get())
                      .AddRegularUser(kPrimaryUserAccountId));
      user_manager_->SetUserPolicyStatus(kPrimaryUserAccountId,
                                         /*is_managed=*/true,
                                         /*is_affiliated=*/true);

      ash::ScopedAccountIdAnnotator annotator(
          profile_manager_.profile_manager(), kPrimaryUserAccountId);
      primary_profile_ =
          profile_manager_.CreateTestingProfile(kPrimaryProfileName);

      user_manager_->OnUserProfileCreated(kPrimaryUserAccountId,
                                          primary_profile_->GetPrefs());
    }
    {
      ASSERT_TRUE(user_manager::TestHelper(user_manager_.Get())
                      .AddRegularUser(kSecondaryUserAccountId));
      user_manager_->SetUserPolicyStatus(kSecondaryUserAccountId,
                                         /*is_managed=*/true,
                                         /*is_affiliated=*/true);

      ash::ScopedAccountIdAnnotator annotator(
          profile_manager_.profile_manager(), kSecondaryUserAccountId);
      secondary_profile_ =
          profile_manager_.CreateTestingProfile(kSecondaryProfileName);
      user_manager_->OnUserProfileCreated(kSecondaryUserAccountId,
                                          secondary_profile_->GetPrefs());
    }

    session_manager_->CreateSession(
        kPrimaryUserAccountId,
        user_manager::TestHelper::GetFakeUsernameHash(kPrimaryUserAccountId),
        /*new_user=*/false, /*has_active_session=*/false);

    EXPECT_TRUE(ash::ProfileHelper::IsPrimaryProfile(primary_profile_));
    EXPECT_FALSE(ash::ProfileHelper::IsPrimaryProfile(secondary_profile_));
  }

  void TearDown() override {
    user_manager_->OnUserProfileWillBeDestroyed(kPrimaryUserAccountId);
    user_manager_->OnUserProfileWillBeDestroyed(kSecondaryUserAccountId);
    primary_profile_ = nullptr;
    secondary_profile_ = nullptr;
    profile_manager_.DeleteAllTestingProfiles();

    session_manager_.reset();
    user_manager_.Reset();
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
  std::unique_ptr<session_manager::SessionManager> session_manager_;
  user_manager::ScopedUserManager user_manager_;
  raw_ptr<TestingProfile> primary_profile_ = nullptr;
  raw_ptr<TestingProfile> secondary_profile_ = nullptr;
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
