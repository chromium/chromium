// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class PrintCompositorLPACSandboxEnabledTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<
          /*policy::key::kPrintingLPACSandboxEnabled=*/std::optional<bool>> {
 public:
  // InProcessBrowserTest implementation:
  void SetUp() override {
    policy_provider_.SetDefaultReturns(
        true /* is_initialization_complete_return */,
        true /* is_first_policy_load_complete_return */);
    policy::PolicyMap values;
    if (GetParam().has_value()) {
      values.Set(policy::key::kPrintingLPACSandboxEnabled,
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

IN_PROC_BROWSER_TEST_P(PrintCompositorLPACSandboxEnabledTest, IsRespected) {
  // The default here is for the LPAC sandbox for the Print Compositor to be
  // enabled, unless it's disabled by policy.
  bool lpac_sandbox_enabled = GetParam().value_or(true);
  ChromeContentBrowserClient client;
  EXPECT_EQ(lpac_sandbox_enabled,
            !client.IsAppContainerDisabled(
                sandbox::mojom::Sandbox::kPrintCompositor));
}

INSTANTIATE_TEST_SUITE_P(
    Enabled,
    PrintCompositorLPACSandboxEnabledTest,
    ::testing::Values(
        /*policy::key::kPrintingLPACSandboxEnabled=*/true));

INSTANTIATE_TEST_SUITE_P(
    Disabled,
    PrintCompositorLPACSandboxEnabledTest,
    ::testing::Values(
        /*policy::key::kPrintingLPACSandboxEnabled=*/false));

INSTANTIATE_TEST_SUITE_P(
    NotSet,
    PrintCompositorLPACSandboxEnabledTest,
    ::testing::Values(
        /*policy::key::kPrintingLPACSandboxEnabled=*/std::nullopt));

}  // namespace policy
