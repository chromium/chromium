// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "third_party/blink/public/common/features.h"

namespace policy {

enum class Policy {
  kDefault,
  kTrue,
  kFalse,
};

class CSSCustomStateDeprecatedSyntaxEnabledPolicyBrowserTest
    : public PolicyTest,
      public ::testing::WithParamInterface<Policy> {
 public:
  static std::string DescribeParams(
      const ::testing::TestParamInfo<ParamType>& info) {
    switch (info.param) {
      case Policy::kDefault:
        return "Default";
      case Policy::kTrue:
        return "True";
      case Policy::kFalse:
        return "False";
    }
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    feature_list_.InitAndDisableFeature(
        blink::features::kCSSCustomStateDeprecatedSyntax);
  }

  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();

    if (GetParam() == Policy::kDefault) {
      return;
    }
    PolicyMap policies;
    SetPolicy(&policies, key::kCSSCustomStateDeprecatedSyntaxEnabled,
              base::Value(GetParam() == Policy::kTrue));
    UpdateProviderPolicy(policies);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(CSSCustomStateDeprecatedSyntaxEnabledPolicyBrowserTest,
                       PolicyIsFollowed) {
  // Both false and the default (no parameter) should be disabled.
  const bool expected_enabled = GetParam() == Policy::kTrue;

  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL(
      "/css_custom_state_deprecated_syntax.html"));
  ASSERT_TRUE(NavigateToUrl(url, this));

  content::DOMMessageQueue message_queue(
      chrome_test_utils::GetActiveWebContents(this));
  std::string enabled_color = "\"rgb(255, 0, 0)\"";
  std::string disabled_color = "\"rgb(0, 0, 255)\"";

  content::ExecuteScriptAsync(
      chrome_test_utils::GetActiveWebContents(this),
      "window.domAutomationController.send(window.deprecatedSyntaxColor)");
  std::string deprecated_syntax_color;
  ASSERT_TRUE(message_queue.WaitForMessage(&deprecated_syntax_color));
  std::string expected_color =
      expected_enabled ? enabled_color : disabled_color;
  EXPECT_EQ(deprecated_syntax_color, expected_color);

  // The new syntax should always be enabled and should not be affected by this
  // enterprise policy.
  content::ExecuteScriptAsync(
      chrome_test_utils::GetActiveWebContents(this),
      "window.domAutomationController.send(window.newSyntaxColor)");
  std::string new_syntax_color;
  ASSERT_TRUE(message_queue.WaitForMessage(&new_syntax_color));
  EXPECT_EQ(new_syntax_color, enabled_color);
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    CSSCustomStateDeprecatedSyntaxEnabledPolicyBrowserTest,
    ::testing::Values(Policy::kDefault, Policy::kTrue, Policy::kFalse),
    &CSSCustomStateDeprecatedSyntaxEnabledPolicyBrowserTest::DescribeParams);

}  // namespace policy
