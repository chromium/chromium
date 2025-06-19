// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/gl/gl_switches.h"

namespace {
//
// Checks if WebGL is enabled in the given WebContents.
bool IsWebGLEnabled(content::WebContents* contents) {
  return content::EvalJs(contents,
                         "var canvas = document.createElement('canvas');"
                         "var context = canvas.getContext('webgl');"
                         "context != null;")
      .ExtractBool();
}

class SwiftShaderEnabledBrowserTest : public PlatformBrowserTest {
 public:
  SwiftShaderEnabledBrowserTest(const SwiftShaderEnabledBrowserTest&) = delete;
  SwiftShaderEnabledBrowserTest& operator=(
      const SwiftShaderEnabledBrowserTest&) = delete;

 protected:
  SwiftShaderEnabledBrowserTest() = default;

  void SetUpInProcessBrowserTestFixture() override {
    // We setup the policy here, because the policy must be 'live' before
    // any GPU initialization since the value for this policy is passed to the
    // GPU process via a command-line.Setting the policy in the test itself or
    // in SetUpOnMainThread works for update-able policies, but is too late for
    // this one.
    provider_.SetDefaultReturns(
        true /* is_initialization_complete_return */,
        true /* is_first_policy_load_complete_return */);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);

    policy::PolicyMap values;
    values.Set(policy::key::kEnableUnsafeSwiftShader,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
    provider_.UpdateChromePolicy(values);
  }
  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
};

IN_PROC_BROWSER_TEST_F(SwiftShaderEnabledBrowserTest, EnableUnsafeSwiftShader) {
  // Verify that WebGL is always available when EnableUnsafeSwiftShader is set,
  // regardless of GPU hardware acceleration availability.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(IsWebGLEnabled(contents));

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  EXPECT_TRUE(command_line->HasSwitch(switches::kEnableUnsafeSwiftShader));
}

}  // namespace
