// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "sandbox/policy/features.h"
#endif

namespace policy {

class NetworkServiceSandboxEnabledTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<
          /*policy::key::kNetworkServiceSandboxEnabled=*/std::optional<bool>> {
 public:
  // InProcessBrowserTest implementation:
  void SetUp() override {
    policy_provider_.SetDefaultReturns(
        true /* is_initialization_complete_return */,
        true /* is_first_policy_load_complete_return */);
    policy::PolicyMap values;
    if (GetParam().has_value()) {
      values.Set(policy::key::kNetworkServiceSandboxEnabled,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                 policy::POLICY_SOURCE_CLOUD, base::Value(*GetParam()),
                 nullptr);
    }
    policy_provider_.UpdateChromePolicy(values);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
    InProcessBrowserTest::SetUp();
  }

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

IN_PROC_BROWSER_TEST_P(NetworkServiceSandboxEnabledTest, IsRespected) {
  content::ContentBrowserClient content_client;
  // Policy always overrides the default.
  bool expected_value =
      GetParam().value_or(content_client.ShouldSandboxNetworkService());
#if BUILDFLAG(IS_WIN)
  // On Windows, the policy is ignored if the platform does not support
  // sandboxing at all, e.g. pre Windows 10.
  if (!sandbox::policy::features::IsNetworkSandboxSupported()) {
    expected_value = false;
  }
#endif
  ChromeContentBrowserClient client;
  EXPECT_EQ(expected_value, client.ShouldSandboxNetworkService());
}

INSTANTIATE_TEST_SUITE_P(
    Enabled,
    NetworkServiceSandboxEnabledTest,
    ::testing::Values(/*policy::key::kNetworkServiceSandboxEnabled=*/true));

INSTANTIATE_TEST_SUITE_P(
    Disabled,
    NetworkServiceSandboxEnabledTest,
    ::testing::Values(/*policy::key::kNetworkServiceSandboxEnabled=*/false));

INSTANTIATE_TEST_SUITE_P(
    NotSet,
    NetworkServiceSandboxEnabledTest,
    ::testing::Values(
        /*policy::key::kNetworkServiceSandboxEnabled=*/std::nullopt));

}  // namespace policy
