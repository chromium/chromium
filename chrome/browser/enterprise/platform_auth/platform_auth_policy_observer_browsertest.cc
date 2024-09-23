// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/platform_auth/platform_auth_policy_observer.h"

#include <vector>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/platform_auth/platform_auth_provider_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

#if BUILDFLAG(IS_MAC)
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/platform_auth/platform_auth_features.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#endif  //  BUILDFLAG(IS_MAC)

class PlatformAuthPolicyObserverTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
    InProcessBrowserTest::SetUp();
  }

 protected:
  PlatformAuthPolicyObserverTest() = default;

  std::optional<PlatformAuthPolicyObserver> platform_auth_policy_observer_;
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
#if BUILDFLAG(IS_MAC)
  base::test::ScopedFeatureList feature_list_{
      enterprise_auth::kEnableExtensibleEnterpriseSSO};
  policy::ScopedManagementServiceOverrideForTesting platform_management_{
      policy::ManagementServiceFactory::GetForPlatform(),
      policy::EnterpriseManagementAuthority::COMPUTER_LOCAL};
#endif  //  BUILDFLAG(IS_MAC)
};

#if BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(PlatformAuthPolicyObserverTest, EnableThenDisable) {
  // Initialize the policy handler.
  PrefService* prefs = g_browser_process->local_state();
  if (prefs)
    platform_auth_policy_observer_.emplace(prefs);

  auto& manager = enterprise_auth::PlatformAuthProviderManager::GetInstance();
  // The manager should be disabled by default since the policy is disabled.
  ASSERT_FALSE(manager.IsEnabled());

  EXPECT_EQ(/*Disabled*/ 0, prefs->GetInteger(prefs::kCloudApAuthEnabled));
  EXPECT_FALSE(prefs->IsManagedPreference(prefs::kCloudApAuthEnabled));

  // Enable the policy.
  policy::PolicyMap policies;
  policies.Set(policy::key::kCloudAPAuthEnabled, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_CLOUD,
               base::Value(1), nullptr);
  policy_provider_.UpdateChromePolicy(policies);

  EXPECT_EQ(/*Enabled*/ 1, prefs->GetInteger(prefs::kCloudApAuthEnabled));
  EXPECT_TRUE(prefs->IsManagedPreference(prefs::kCloudApAuthEnabled));

  // The manager should now be enabled.
  ASSERT_TRUE(manager.IsEnabled());

  // Disable the policy.
  policies.Set(policy::key::kCloudAPAuthEnabled, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_CLOUD,
               base::Value(0), nullptr);
  policy_provider_.UpdateChromePolicy(policies);

  EXPECT_EQ(/*Disabled*/ 0, prefs->GetInteger(prefs::kCloudApAuthEnabled));
  EXPECT_TRUE(prefs->IsManagedPreference(prefs::kCloudApAuthEnabled));

  // The manager should now be disabled.
  ASSERT_FALSE(manager.IsEnabled());

  platform_auth_policy_observer_.reset();
}

IN_PROC_BROWSER_TEST_F(PlatformAuthPolicyObserverTest, EnableThenUnset) {
  // Initialize the policy handler.
  PrefService* prefs = g_browser_process->local_state();
  if (prefs)
    platform_auth_policy_observer_.emplace(prefs);

  auto& manager = enterprise_auth::PlatformAuthProviderManager::GetInstance();
  // The manager should be disabled by default since the policy is disabled.
  ASSERT_FALSE(manager.IsEnabled());

  EXPECT_EQ(/*Disabled*/ 0, prefs->GetInteger(prefs::kCloudApAuthEnabled));
  EXPECT_FALSE(prefs->IsManagedPreference(prefs::kCloudApAuthEnabled));

  // Enable the policy.
  policy::PolicyMap policies;
  policies.Set(policy::key::kCloudAPAuthEnabled, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_CLOUD,
               base::Value(1), nullptr);
  policy_provider_.UpdateChromePolicy(policies);

  EXPECT_EQ(/*Enabled*/ 1, prefs->GetInteger(prefs::kCloudApAuthEnabled));
  EXPECT_TRUE(prefs->IsManagedPreference(prefs::kCloudApAuthEnabled));

  // The manager should now be enabled.
  ASSERT_TRUE(manager.IsEnabled());

  // Unset the policy.
  policies.Erase(policy::key::kCloudAPAuthEnabled);
  policy_provider_.UpdateChromePolicy(policies);

  EXPECT_EQ(/*Disabled*/ 0, prefs->GetInteger(prefs::kCloudApAuthEnabled));
  EXPECT_FALSE(prefs->IsManagedPreference(prefs::kCloudApAuthEnabled));

  // The manager should now be disabled.
  ASSERT_FALSE(manager.IsEnabled());

  platform_auth_policy_observer_.reset();
}
#elif BUILDFLAG(IS_MAC)

IN_PROC_BROWSER_TEST_F(PlatformAuthPolicyObserverTest, DisableThenEnable) {
  // Initialize the policy handler.
  PrefService* prefs = g_browser_process->local_state();
  if (prefs) {
    platform_auth_policy_observer_.emplace(prefs);
  }
  auto& manager = enterprise_auth::PlatformAuthProviderManager::GetInstance();
  // The manager should be enabled by default since the policy is enabled.
  ASSERT_TRUE(manager.IsEnabled());

  EXPECT_EQ(/*Enabled*/ 1,
            prefs->GetInteger(prefs::kExtensibleEnterpriseSSOEnabled));
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kExtensibleEnterpriseSSOEnabled));

  // Disable the policy.
  policy::PolicyMap policies;
  policies.Set(policy::key::kExtensibleEnterpriseSSOEnabled,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
               policy::POLICY_SOURCE_CLOUD, base::Value(0), nullptr);
  policy_provider_.UpdateChromePolicy(policies);

  EXPECT_EQ(/*Disabled*/ 0,
            prefs->GetInteger(prefs::kExtensibleEnterpriseSSOEnabled));
  EXPECT_TRUE(
      prefs->IsManagedPreference(prefs::kExtensibleEnterpriseSSOEnabled));

  // The manager should now be disabled.
  ASSERT_FALSE(manager.IsEnabled());

  // Enable the policy.
  policies.Set(policy::key::kExtensibleEnterpriseSSOEnabled,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
               policy::POLICY_SOURCE_CLOUD, base::Value(1), nullptr);
  policy_provider_.UpdateChromePolicy(policies);

  EXPECT_EQ(/*Enabled*/ 1,
            prefs->GetInteger(prefs::kExtensibleEnterpriseSSOEnabled));
  EXPECT_TRUE(
      prefs->IsManagedPreference(prefs::kExtensibleEnterpriseSSOEnabled));

  // The manager should now be enabled.
  ASSERT_TRUE(manager.IsEnabled());

  platform_auth_policy_observer_.reset();
}

IN_PROC_BROWSER_TEST_F(PlatformAuthPolicyObserverTest, DisableThenUnset) {
  // Initialize the policy handler.
  PrefService* prefs = g_browser_process->local_state();
  if (prefs) {
    platform_auth_policy_observer_.emplace(prefs);
  }
  auto& manager = enterprise_auth::PlatformAuthProviderManager::GetInstance();
  // The manager should be enabled by default since the policy is enabled.
  ASSERT_TRUE(manager.IsEnabled());

  EXPECT_EQ(/*Enabled*/ 1,
            prefs->GetInteger(prefs::kExtensibleEnterpriseSSOEnabled));
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kExtensibleEnterpriseSSOEnabled));

  // Disable the policy.
  policy::PolicyMap policies;
  policies.Set(policy::key::kExtensibleEnterpriseSSOEnabled,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
               policy::POLICY_SOURCE_CLOUD, base::Value(0), nullptr);
  policy_provider_.UpdateChromePolicy(policies);

  EXPECT_EQ(/*Disabled*/ 0,
            prefs->GetInteger(prefs::kExtensibleEnterpriseSSOEnabled));
  EXPECT_TRUE(
      prefs->IsManagedPreference(prefs::kExtensibleEnterpriseSSOEnabled));

  // The manager should now be disabled.
  ASSERT_FALSE(manager.IsEnabled());

  // Unset the policy.
  policies.Erase(policy::key::kExtensibleEnterpriseSSOEnabled);
  policy_provider_.UpdateChromePolicy(policies);

  EXPECT_EQ(/*Enabled*/ 1,
            prefs->GetInteger(prefs::kExtensibleEnterpriseSSOEnabled));
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kExtensibleEnterpriseSSOEnabled));

  // The manager should now be disabled.
  ASSERT_TRUE(manager.IsEnabled());

  platform_auth_policy_observer_.reset();
}

IN_PROC_BROWSER_TEST_F(PlatformAuthPolicyObserverTest, UnmanagedDevice) {
  policy::ScopedManagementServiceOverrideForTesting platform_management(
      policy::ManagementServiceFactory::GetForPlatform(),
      policy::EnterpriseManagementAuthority::NONE);
  // Initialize the policy handler.
  PrefService* prefs = g_browser_process->local_state();
  if (prefs) {
    platform_auth_policy_observer_.emplace(prefs);
  }
  auto& manager = enterprise_auth::PlatformAuthProviderManager::GetInstance();
  // The manager should be disabled by default since we are on an unmanaged
  // device.
  ASSERT_FALSE(manager.IsEnabled());

  platform_auth_policy_observer_.reset();
}
#endif  //  BUILDFLAG(IS_WIN)
