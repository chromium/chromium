// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

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

enum class FeatureState {
  kEnabled,
  kDisabled,
};

class XSLTPolicyBrowserTest
    : public PolicyTest,
      public ::testing::WithParamInterface<std::tuple<Policy, FeatureState>> {
 public:
  static std::string DescribeParams(
      const ::testing::TestParamInfo<ParamType>& info) {
    std::string description;
    switch (std::get<0>(info.param)) {
      case Policy::kDefault:
        description += "PolicyDefault";
        break;
      case Policy::kTrue:
        description += "PolicyTrue";
        break;
      case Policy::kFalse:
        description += "PolicyFalse";
        break;
    }
    description += "_";
    switch (std::get<1>(info.param)) {
      case FeatureState::kEnabled:
        description += "FeatureEnabled";
        break;
      case FeatureState::kDisabled:
        description += "FeatureDisabled";
        break;
    }
    return description;
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    FeatureState feature_state = std::get<1>(GetParam());
    if (feature_state == FeatureState::kEnabled) {
      feature_list_.InitWithFeatures({blink::features::kXSLT},
                                     {blink::features::kXSLTSpecialTrial});
    } else {
      feature_list_.InitWithFeatures({}, {blink::features::kXSLT});
    }
  }

  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    Policy policy_val = std::get<0>(GetParam());
    if (policy_val == Policy::kDefault) {
      return;
    }
    PolicyMap policies;
    SetPolicy(&policies, key::kXSLTEnabled,
              base::Value(policy_val == Policy::kTrue));
    UpdateProviderPolicy(policies);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(XSLTPolicyBrowserTest, PolicyIsFollowed) {
  Policy policy_val = std::get<0>(GetParam());
  FeatureState feature_state = std::get<1>(GetParam());

  bool expected_enabled = false;
  if (policy_val == Policy::kTrue) {
    expected_enabled = true;
  } else if (policy_val == Policy::kFalse) {
    expected_enabled = false;
  } else {
    // Default
    expected_enabled = (feature_state == FeatureState::kEnabled);
  }

  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/empty.html"));
  ASSERT_TRUE(NavigateToUrl(url, this));

  content::DOMMessageQueue message_queue(
      chrome_test_utils::GetActiveWebContents(this));
  content::ExecuteScriptAsync(chrome_test_utils::GetActiveWebContents(this),
                              R"(
        try {
          new XSLTProcessor();
          // XSLT Enabled:
          window.domAutomationController.send(true);
        } catch {
          // XSLT Disabled:
          window.domAutomationController.send(false);
        }
      )");
  std::string message;
  EXPECT_TRUE(message_queue.WaitForMessage(&message));
  EXPECT_TRUE(message == "true" || message == "false");
  EXPECT_EQ(message == "true", expected_enabled);
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    XSLTPolicyBrowserTest,
    ::testing::Combine(
        ::testing::Values(Policy::kDefault, Policy::kTrue, Policy::kFalse),
        ::testing::Values(FeatureState::kEnabled, FeatureState::kDisabled)),
    &XSLTPolicyBrowserTest::DescribeParams);

}  // namespace policy
