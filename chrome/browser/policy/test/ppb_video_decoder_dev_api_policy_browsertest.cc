// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"

namespace {

enum class PolicySetting {
  On,
  Off,
  Unset,
};

enum class FeatureSetting {
  On,
  Off,
  Unset,
};

// Subclasses ChromeContentBrowserClient to save and expose command lines that
// are passed to renderer processes.
class ChromeContentBrowserClientForPolicyTests
    : public ChromeContentBrowserClient {
 public:
  ChromeContentBrowserClientForPolicyTests() = default;
  ChromeContentBrowserClientForPolicyTests(
      const ChromeContentBrowserClientForPolicyTests&) = delete;
  ChromeContentBrowserClientForPolicyTests& operator=(
      const ChromeContentBrowserClientForPolicyTests&) = delete;

  ~ChromeContentBrowserClientForPolicyTests() override = default;

  base::CommandLine* command_line_from_last_renderer() {
    return command_line_from_last_renderer_.get();
  }

  void clear_command_line_from_last_renderer() {
    command_line_from_last_renderer_.reset();
  }

 private:
  void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                      int child_process_id) override {
    ChromeContentBrowserClient::AppendExtraCommandLineSwitches(
        command_line, child_process_id);

    if (command_line->GetSwitchValueASCII(switches::kProcessType) ==
        switches::kRendererProcess) {
      command_line_from_last_renderer_ =
          std::make_unique<base::CommandLine>(*command_line);
    }
  }

  std::unique_ptr<base::CommandLine> command_line_from_last_renderer_;
};

}  // namespace

// NOTE: This test is templated rather than parameterized in the interests of
// readability: while a parameterized test would be shorter, it was the author's
// judgment that it would be significantly harder to read and comprehend the
// meaning of what was being tested across the different permutations.
template <PolicySetting policy_setting, FeatureSetting feature_setting>
class PPBVideoDecoderDevAPIPolicyBrowserTest : public PlatformBrowserTest {
 public:
  PPBVideoDecoderDevAPIPolicyBrowserTest(
      const PPBVideoDecoderDevAPIPolicyBrowserTest&) = delete;
  PPBVideoDecoderDevAPIPolicyBrowserTest& operator=(
      const PPBVideoDecoderDevAPIPolicyBrowserTest&) = delete;

 protected:
  PPBVideoDecoderDevAPIPolicyBrowserTest() = default;

  void SetUpInProcessBrowserTestFixture() override {
    // Set the feature to the desired setting (if any).
    if (feature_setting == FeatureSetting::On) {
      feature_list_.InitAndEnableFeature(
          features::kSupportPepperVideoDecoderDevAPI);
    } else if (feature_setting == FeatureSetting::Off) {
      feature_list_.InitAndDisableFeature(
          features::kSupportPepperVideoDecoderDevAPI);
    }

    if (policy_setting == PolicySetting::Unset) {
      return;
    }

    // Set up the policy here as the policy must be 'live' before the renderer
    // is created, since the value for this policy is passed to the renderer via
    // a command-line. Setting the policy in the test itself or in
    // SetUpOnMainThread works for update-able policies, but is too late for
    // this one.
    provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);

    policy::PolicyMap values;

    const char* kPolicyName = policy::key::kForceEnablePepperVideoDecoderDevAPI;
    values.Set(kPolicyName, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               base::Value(policy_setting == PolicySetting::On), nullptr);
    provider_.UpdateChromePolicy(values);
  }

  void SetUpOnMainThread() override {
    content::SetBrowserClientForTesting(&test_browser_client_);
  }

  // Creates a new renderer process.
  void CreateRenderer() {
    test_browser_client_.clear_command_line_from_last_renderer();
    ASSERT_FALSE(command_line_from_last_renderer());

    ASSERT_TRUE(
        AddTabAtIndex(0, GURL("chrome://version"), ui::PAGE_TRANSITION_TYPED));
    ASSERT_TRUE(command_line_from_last_renderer());
  }

  // Returns whether the last-created renderer was passed the command-line
  // switch for force-enabling the PPB_VideoDecoder(Dev) API.
  bool renderer_was_passed_policy_switch() {
    return command_line_from_last_renderer()->HasSwitch(
        switches::kForceEnablePepperVideoDecoderDevAPI);
  }

 private:
  base::CommandLine* command_line_from_last_renderer() {
    return test_browser_client_.command_line_from_last_renderer();
  }

  ChromeContentBrowserClientForPolicyTests test_browser_client_;
  base::test::ScopedFeatureList feature_list_;
  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
};

// Tests of the policy being turned on: the command-line switch should be
// present, regardless of the setting of the base::Feature.
typedef PPBVideoDecoderDevAPIPolicyBrowserTest<PolicySetting::On,
                                               FeatureSetting::On>
    PPBVideoDecoderDevAPIPolicyEnabledFeatureEnabledBrowserTest;
typedef PPBVideoDecoderDevAPIPolicyBrowserTest<PolicySetting::On,
                                               FeatureSetting::Off>
    PPBVideoDecoderDevAPIPolicyEnabledFeatureDisabledBrowserTest;
typedef PPBVideoDecoderDevAPIPolicyBrowserTest<PolicySetting::On,
                                               FeatureSetting::Unset>
    PPBVideoDecoderDevAPIPolicyEnabledFeatureNotSetBrowserTest;

IN_PROC_BROWSER_TEST_F(
    PPBVideoDecoderDevAPIPolicyEnabledFeatureEnabledBrowserTest,
    CommandLineSwitchPresent) {
  CreateRenderer();
  EXPECT_TRUE(renderer_was_passed_policy_switch());
}

IN_PROC_BROWSER_TEST_F(
    PPBVideoDecoderDevAPIPolicyEnabledFeatureDisabledBrowserTest,
    CommandLineSwitchPresent) {
  CreateRenderer();
  EXPECT_TRUE(renderer_was_passed_policy_switch());
}

IN_PROC_BROWSER_TEST_F(
    PPBVideoDecoderDevAPIPolicyEnabledFeatureNotSetBrowserTest,
    CommandLineSwitchPresent) {
  CreateRenderer();
  EXPECT_TRUE(renderer_was_passed_policy_switch());
}

// Tests of the policy being turned off: the command-line switch should be
// missing, regardless of the setting of the base::Feature.
typedef PPBVideoDecoderDevAPIPolicyBrowserTest<PolicySetting::Off,
                                               FeatureSetting::On>
    PPBVideoDecoderDevAPIPolicyDisabledFeatureEnabledBrowserTest;
typedef PPBVideoDecoderDevAPIPolicyBrowserTest<PolicySetting::Off,
                                               FeatureSetting::Off>
    PPBVideoDecoderDevAPIPolicyDisabledFeatureDisabledBrowserTest;
typedef PPBVideoDecoderDevAPIPolicyBrowserTest<PolicySetting::Off,
                                               FeatureSetting::Unset>
    PPBVideoDecoderDevAPIPolicyDisabledFeatureNotSetBrowserTest;

IN_PROC_BROWSER_TEST_F(
    PPBVideoDecoderDevAPIPolicyDisabledFeatureEnabledBrowserTest,
    CommandLineSwitchNotPresent) {
  CreateRenderer();
  EXPECT_FALSE(renderer_was_passed_policy_switch());
}

IN_PROC_BROWSER_TEST_F(
    PPBVideoDecoderDevAPIPolicyDisabledFeatureDisabledBrowserTest,
    CommandLineSwitchNotPresent) {
  CreateRenderer();
  EXPECT_FALSE(renderer_was_passed_policy_switch());
}

IN_PROC_BROWSER_TEST_F(
    PPBVideoDecoderDevAPIPolicyDisabledFeatureNotSetBrowserTest,
    CommandLineSwitchNotPresent) {
  CreateRenderer();
  EXPECT_FALSE(renderer_was_passed_policy_switch());
}

// Tests of the policy not being set: the command-line switch should be missing,
// regardless of the setting of the base::Feature.
typedef PPBVideoDecoderDevAPIPolicyBrowserTest<PolicySetting::Unset,
                                               FeatureSetting::On>
    PPBVideoDecoderDevAPIPolicyNotSetFeatureEnabledBrowserTest;
typedef PPBVideoDecoderDevAPIPolicyBrowserTest<PolicySetting::Unset,
                                               FeatureSetting::Off>
    PPBVideoDecoderDevAPIPolicyNotSetFeatureDisabledBrowserTest;
typedef PPBVideoDecoderDevAPIPolicyBrowserTest<PolicySetting::Unset,
                                               FeatureSetting::Unset>
    PPBVideoDecoderDevAPIPolicyNotSetFeatureNotSetBrowserTest;

IN_PROC_BROWSER_TEST_F(
    PPBVideoDecoderDevAPIPolicyNotSetFeatureEnabledBrowserTest,
    CommandLineSwitchNotPresent) {
  CreateRenderer();
  EXPECT_FALSE(renderer_was_passed_policy_switch());
}

IN_PROC_BROWSER_TEST_F(
    PPBVideoDecoderDevAPIPolicyNotSetFeatureDisabledBrowserTest,
    CommandLineSwitchNotPresent) {
  CreateRenderer();
  EXPECT_FALSE(renderer_was_passed_policy_switch());
}

IN_PROC_BROWSER_TEST_F(
    PPBVideoDecoderDevAPIPolicyNotSetFeatureNotSetBrowserTest,
    CommandLineSwitchNotPresent) {
  CreateRenderer();
  EXPECT_FALSE(renderer_was_passed_policy_switch());
}
