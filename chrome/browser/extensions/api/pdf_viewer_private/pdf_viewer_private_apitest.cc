// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"

namespace extensions {

namespace {

base::Value::List GenerateSampleAllowlist() {
  base::Value::List allowlist;
  allowlist.Append("allowed-domain.com");
  allowlist.Append("example.com");

  // Even though the allowlist is supposed to be a set of domains, it's possible
  // for it to contain garbage values. This should be handled gracefully.
  allowlist.Append(10);
  return allowlist;
}

}  // namespace

class PdfViewerPrivateApiTest : public ExtensionApiTest {
 public:
  // Set up policy values.
  void SetUpInProcessBrowserTestFixture() override {
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);

    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
    policy::PolicyMap values;

    values.Set(policy::key::kPdfLocalFileAccessAllowedForDomains,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD,
               base::Value(GenerateSampleAllowlist()), nullptr);
    policy_provider_.UpdateChromePolicy(values);
  }

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

IN_PROC_BROWSER_TEST_F(PdfViewerPrivateApiTest, All) {
  ASSERT_TRUE(RunExtensionTest("pdf_viewer_private")) << message_;
}

}  // namespace extensions
