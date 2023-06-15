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

  absl::optional<PlatformAuthPolicyObserver> platform_auth_policy_observer_;
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

IN_PROC_BROWSER_TEST_F(PlatformAuthPolicyObserverTest, EnableThenDisable) {
  auto& manager = enterprise_auth::PlatformAuthProviderManager::GetInstance();
  // The manager should be disabled by default since the policy is disabled.
  ASSERT_FALSE(manager.IsEnabled());

  // Initialize the policy handler.
  PrefService* prefs = g_browser_process->local_state();
  if (prefs)
    platform_auth_policy_observer_.emplace(prefs);

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
  auto& manager = enterprise_auth::PlatformAuthProviderManager::GetInstance();
  // The manager should be disabled by default since the policy is disabled.
  ASSERT_FALSE(manager.IsEnabled());

  // Initialize the policy handler.
  PrefService* prefs = g_browser_process->local_state();
  if (prefs)
    platform_auth_policy_observer_.emplace(prefs);

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
