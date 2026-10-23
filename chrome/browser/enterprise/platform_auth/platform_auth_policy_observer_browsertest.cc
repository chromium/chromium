// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/platform_auth/platform_auth_policy_observer.h"

#include <vector>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/platform_auth/platform_auth_provider_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

#if BUILDFLAG(IS_MAC)
#include "base/values.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/platform_auth/extensible_enterprise_sso_metadata.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#endif  //  BUILDFLAG(IS_MAC)

namespace {

#if BUILDFLAG(IS_WIN)
const char* kPrefKey = prefs::kCloudApAuthEnabled;
const char* kPolicyKey = policy::key::kCloudAPAuthEnabled;
#elif BUILDFLAG(IS_ANDROID)
const char* kPrefKey = prefs::kAndroidEntraSSOEnabled;
const char* kPolicyKey = policy::key::kAndroidEntraSsoEnabled;
#endif

}  // namespace

// The `PlatformAuthPolicyObserver` under test is the one owned by
// `ChromeBrowserMainParts`, so any state it depends on (features, management
// state) has to be set up before the browser starts.
class PlatformAuthPolicyObserverTest : public PlatformBrowserTest {
 public:
  void SetUp() override {
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
    PlatformBrowserTest::SetUp();
  }

 protected:
  PlatformAuthPolicyObserverTest() = default;

  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
#if BUILDFLAG(IS_MAC)
  policy::ScopedManagementServiceOverrideForTesting platform_management_{
      policy::ManagementServiceFactory::GetForPlatform(),
      policy::EnterpriseManagementAuthority::COMPUTER_LOCAL};
#endif  //  BUILDFLAG(IS_MAC)
};

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(PlatformAuthPolicyObserverTest, EnableThenDisable) {
  PrefService* prefs = g_browser_process->local_state();
  ASSERT_TRUE(prefs);

  auto& manager = enterprise_auth::PlatformAuthProviderManager::GetInstance();
  // The manager should be disabled by default since the policy is disabled.
  ASSERT_FALSE(manager.IsEnabled());

  EXPECT_EQ(/*Disabled*/ 0, prefs->GetInteger(kPrefKey));
  EXPECT_FALSE(prefs->IsManagedPreference(kPrefKey));

  // Enable the policy.
  policy::PolicyMap policies;
  policies.Set(kPolicyKey, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_CLOUD,
               base::Value(1), nullptr);
  policy_provider_.UpdateChromePolicy(policies);

  EXPECT_EQ(/*Enabled*/ 1, prefs->GetInteger(kPrefKey));
  EXPECT_TRUE(prefs->IsManagedPreference(kPrefKey));

  // The manager should now be enabled.
  ASSERT_TRUE(manager.IsEnabled());

  // Disable the policy.
  policies.Set(kPolicyKey, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_CLOUD,
               base::Value(0), nullptr);
  policy_provider_.UpdateChromePolicy(policies);

  EXPECT_EQ(/*Disabled*/ 0, prefs->GetInteger(kPrefKey));
  EXPECT_TRUE(prefs->IsManagedPreference(kPrefKey));

  // The manager should now be disabled.
  ASSERT_FALSE(manager.IsEnabled());
}

IN_PROC_BROWSER_TEST_F(PlatformAuthPolicyObserverTest, EnableThenUnset) {
  PrefService* prefs = g_browser_process->local_state();
  ASSERT_TRUE(prefs);

  auto& manager = enterprise_auth::PlatformAuthProviderManager::GetInstance();
  // The manager should be disabled by default since the policy is disabled.
  ASSERT_FALSE(manager.IsEnabled());

  EXPECT_EQ(/*Disabled*/ 0, prefs->GetInteger(kPrefKey));
  EXPECT_FALSE(prefs->IsManagedPreference(kPrefKey));

  // Enable the policy.
  policy::PolicyMap policies;
  policies.Set(kPolicyKey, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_CLOUD,
               base::Value(1), nullptr);
  policy_provider_.UpdateChromePolicy(policies);

  EXPECT_EQ(/*Enabled*/ 1, prefs->GetInteger(kPrefKey));
  EXPECT_TRUE(prefs->IsManagedPreference(kPrefKey));

  // The manager should now be enabled.
  ASSERT_TRUE(manager.IsEnabled());

  // Unset the policy.
  policies.Erase(kPolicyKey);
  policy_provider_.UpdateChromePolicy(policies);

  EXPECT_EQ(/*Disabled*/ 0, prefs->GetInteger(kPrefKey));
  EXPECT_FALSE(prefs->IsManagedPreference(kPrefKey));

  // The manager should now be disabled.
  ASSERT_FALSE(manager.IsEnabled());
}

// Sets the policy before the browser starts, so that it is already applied
// when the browser-owned `PlatformAuthPolicyObserver` is created.
class PlatformAuthPolicyObserverPolicySetBeforeStartupTest
    : public PlatformAuthPolicyObserverTest {
 protected:
  void SetUpInProcessBrowserTestFixture() override {
    PlatformAuthPolicyObserverTest::SetUpInProcessBrowserTestFixture();
    policy::PolicyMap policies;
    policies.Set(kPolicyKey, policy::POLICY_LEVEL_MANDATORY,
                 policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_CLOUD,
                 base::Value(1), nullptr);
    policy_provider_.UpdateChromePolicy(policies);
  }
};

// Tests that the manager is enabled right away when the policy is already set
// when the observer is created.
IN_PROC_BROWSER_TEST_F(PlatformAuthPolicyObserverPolicySetBeforeStartupTest,
                       EnabledWhenPolicySetBeforeCreation) {
  PrefService* prefs = g_browser_process->local_state();
  ASSERT_TRUE(prefs);
  ASSERT_EQ(/*Enabled*/ 1, prefs->GetInteger(kPrefKey));

  EXPECT_TRUE(
      enterprise_auth::PlatformAuthProviderManager::GetInstance().IsEnabled());
}

#elif BUILDFLAG(IS_MAC)

IN_PROC_BROWSER_TEST_F(PlatformAuthPolicyObserverTest, DisableThenEnable) {
  PrefService* prefs = g_browser_process->local_state();
  ASSERT_TRUE(prefs);

  auto& manager = enterprise_auth::PlatformAuthProviderManager::GetInstance();
  // The manager should be enabled by default since the policy is enabled.
  ASSERT_TRUE(manager.IsEnabled());

  EXPECT_EQ(/*Enabled*/ 1,
            prefs->GetInteger(prefs::kExtensibleEnterpriseSSOEnabled));
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kExtensibleEnterpriseSSOEnabled));

  // Disable the policy.
  policy::PolicyMap policies;
  base::Value extensibleEnterpriseSSOBlocklistValues(base::Value::Type::LIST);
  extensibleEnterpriseSSOBlocklistValues.GetList().Append("all");
  policies.Set(policy::key::kExtensibleEnterpriseSSOBlocklist,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
               policy::POLICY_SOURCE_CLOUD,
               std::move(extensibleEnterpriseSSOBlocklistValues), nullptr);
  policy_provider_.UpdateChromePolicy(policies);

  EXPECT_EQ(/*Disabled*/ 0,
            prefs->GetInteger(prefs::kExtensibleEnterpriseSSOEnabled));
  EXPECT_TRUE(
      prefs->IsManagedPreference(prefs::kExtensibleEnterpriseSSOEnabled));

  // The manager should now be disabled.
  ASSERT_FALSE(manager.IsEnabled());

  // Enable the policy.
  policies.Set(policy::key::kExtensibleEnterpriseSSOBlocklist,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
               policy::POLICY_SOURCE_CLOUD,
               base::Value(base::Value::Type::LIST), nullptr);
  policy_provider_.UpdateChromePolicy(policies);

  EXPECT_EQ(/*Enabled*/ 1,
            prefs->GetInteger(prefs::kExtensibleEnterpriseSSOEnabled));
  EXPECT_TRUE(
      prefs->IsManagedPreference(prefs::kExtensibleEnterpriseSSOEnabled));

  // The manager should now be enabled.
  ASSERT_TRUE(manager.IsEnabled());
}

IN_PROC_BROWSER_TEST_F(PlatformAuthPolicyObserverTest, DisableThenUnset) {
  PrefService* prefs = g_browser_process->local_state();
  ASSERT_TRUE(prefs);

  auto& manager = enterprise_auth::PlatformAuthProviderManager::GetInstance();
  // The manager should be enabled by default since the policy is enabled.
  ASSERT_TRUE(manager.IsEnabled());

  EXPECT_EQ(/*Enabled*/ 1,
            prefs->GetInteger(prefs::kExtensibleEnterpriseSSOEnabled));
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kExtensibleEnterpriseSSOEnabled));

  // Disable the policy.
  policy::PolicyMap policies;
  base::Value extensibleEnterpriseSSOBlocklistValues(base::Value::Type::LIST);
  extensibleEnterpriseSSOBlocklistValues.GetList().Append("all");
  policies.Set(policy::key::kExtensibleEnterpriseSSOBlocklist,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
               policy::POLICY_SOURCE_CLOUD,
               std::move(extensibleEnterpriseSSOBlocklistValues), nullptr);
  policy_provider_.UpdateChromePolicy(policies);

  EXPECT_EQ(/*Disabled*/ 0,
            prefs->GetInteger(prefs::kExtensibleEnterpriseSSOEnabled));
  EXPECT_TRUE(
      prefs->IsManagedPreference(prefs::kExtensibleEnterpriseSSOEnabled));

  // The manager should now be disabled.
  ASSERT_FALSE(manager.IsEnabled());

  // Unset the policy.
  policies.Erase(policy::key::kExtensibleEnterpriseSSOBlocklist);
  policy_provider_.UpdateChromePolicy(policies);

  EXPECT_EQ(/*Enabled*/ 1,
            prefs->GetInteger(prefs::kExtensibleEnterpriseSSOEnabled));
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kExtensibleEnterpriseSSOEnabled));

  // The manager should now be disabled.
  ASSERT_TRUE(manager.IsEnabled());
}

// Simulates an unmanaged device. The override is a member so that it takes
// effect before the browser-owned `PlatformAuthPolicyObserver` is created, and
// it is declared after the base fixture's override so it takes precedence.
class PlatformAuthPolicyObserverUnmanagedTest
    : public PlatformAuthPolicyObserverTest {
 protected:
  policy::ScopedManagementServiceOverrideForTesting unmanaged_platform_{
      policy::ManagementServiceFactory::GetForPlatform(),
      policy::EnterpriseManagementAuthority::NONE};
};

IN_PROC_BROWSER_TEST_F(PlatformAuthPolicyObserverUnmanagedTest,
                       UnmanagedDevice) {
  auto& manager = enterprise_auth::PlatformAuthProviderManager::GetInstance();
  // The manager should be disabled by default since we are on an unmanaged
  // device.
  ASSERT_FALSE(manager.IsEnabled());
}

// Tests that a blocklist which leaves some IdPs allowed keeps the manager
// enabled. The pref value stays `1` but becomes managed, which still triggers
// a pref change notification.
IN_PROC_BROWSER_TEST_F(PlatformAuthPolicyObserverTest,
                       PartialBlocklistKeepsManagerEnabled) {
  PrefService* prefs = g_browser_process->local_state();
  ASSERT_TRUE(prefs);
  auto& manager = enterprise_auth::PlatformAuthProviderManager::GetInstance();
  ASSERT_TRUE(manager.IsEnabled());

  policy::PolicyMap policies;
  base::ListValue blocklist;
  blocklist.Append(enterprise_auth::kOktaIdentityProvider);
  policies.Set(policy::key::kExtensibleEnterpriseSSOBlocklist,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
               policy::POLICY_SOURCE_CLOUD, base::Value(std::move(blocklist)),
               nullptr);
  policy_provider_.UpdateChromePolicy(policies);

  EXPECT_EQ(/*Enabled*/ 1,
            prefs->GetInteger(prefs::kExtensibleEnterpriseSSOEnabled));
  EXPECT_TRUE(
      prefs->IsManagedPreference(prefs::kExtensibleEnterpriseSSOEnabled));
  EXPECT_TRUE(manager.IsEnabled());
}
#endif  //  BUILDFLAG(IS_MAC)
