// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_reg_util_win.h"
#include "base/values.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/win/isolated_browser/isolated_browser_support.h"
#include "chrome/install_static/test/scoped_install_details.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class ProcessIsolationEnabledBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    registry_override_.OverrideRegistry(HKEY_CURRENT_USER);
    scoped_install_details_ =
        std::make_unique<install_static::ScopedInstallDetails>(
            /*system_level=*/true);

    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);

    policy::PolicyMap values;
    values.Set(policy::key::kProcessIsolationEnabled,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
               policy::POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
    policy_provider_.UpdateChromePolicy(values);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);

    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  }

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
  registry_util::RegistryOverrideManager registry_override_;
  std::unique_ptr<install_static::ScopedInstallDetails> scoped_install_details_;
};

IN_PROC_BROWSER_TEST_F(ProcessIsolationEnabledBrowserTest,
                       PolicyAppliesToBackend) {
  // Wait for the policy to apply to the backend via UpdateProcessIsolationState
  // which does it asynchronously on a background task.
  EXPECT_TRUE(
      base::test::RunUntil([]() { return chrome::IsIsolationEnabled(); }));
}

class ProcessIsolationEnabledPolicyWithFieldTrialBrowserTest
    : public ProcessIsolationEnabledBrowserTest {
 public:
  ProcessIsolationEnabledPolicyWithFieldTrialBrowserTest() {
    auto feature_list = std::make_unique<base::FeatureList>();
    // Register a field trial and tie it to the disabled feature.
    feature_list->RegisterFieldTrialOverride(
        features::kIsolatedProcess.name,
        base::FeatureList::OVERRIDE_DISABLE_FEATURE,
        base::FieldTrialList::CreateFieldTrial("TestIsolationStudy",
                                               "Disabled"));
    feature_list_.InitWithFeatureList(std::move(feature_list));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ProcessIsolationEnabledPolicyWithFieldTrialBrowserTest,
                       PolicyOverridesFieldTrial) {
  EXPECT_TRUE(
      base::test::RunUntil([]() { return chrome::IsIsolationEnabled(); }));
}

}  // namespace policy
