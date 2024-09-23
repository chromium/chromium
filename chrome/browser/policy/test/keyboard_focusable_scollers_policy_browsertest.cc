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

class KeyboardFocusableScrollersEnabledPolicyBrowserTest
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
  base::test::ScopedFeatureList feature_list_;
  void SetUpCommandLine(base::CommandLine* command_line) override {
    feature_list_.InitAndEnableFeature(
        blink::features::kKeyboardFocusableScrollers);
  }

  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();

    if (GetParam() == Policy::kDefault) {
      return;
    }
    PolicyMap policies;
    SetPolicy(&policies, key::kKeyboardFocusableScrollersEnabled,
              base::Value(GetParam() == Policy::kTrue));
    UpdateProviderPolicy(policies);
  }
};

IN_PROC_BROWSER_TEST_P(KeyboardFocusableScrollersEnabledPolicyBrowserTest,
                       PolicyIsFollowed) {
  // Both true and the default (no parameter) should be enabled.
  const bool expect_disabled = GetParam() == Policy::kFalse;

  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/empty.html"));
  ASSERT_TRUE(NavigateToUrl(url, this));

  content::DOMMessageQueue message_queue(
      chrome_test_utils::GetActiveWebContents(this));
  content::ExecuteScriptAsync(chrome_test_utils::GetActiveWebContents(this),
                              R"(
    const scroller = document.createElement("div");
    scroller.style = 'grey; width: 100px; height: 100px; overflow: auto';
    const content = document.createElement("div");
    content.style = 'width: 200px; height: 200px';
    scroller.appendChild(content);
    document.body.appendChild(scroller);
    scroller.focus();
    if (document.activeElement == scroller) {
      window.domAutomationController.send(true);
    } else {
      window.domAutomationController.send(false);
    }
  )");
  std::string message;
  EXPECT_TRUE(message_queue.WaitForMessage(&message));
  EXPECT_TRUE(message == "true" || message == "false");
  EXPECT_EQ(message == "false", expect_disabled);
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    KeyboardFocusableScrollersEnabledPolicyBrowserTest,
    ::testing::Values(Policy::kDefault, Policy::kTrue, Policy::kFalse),
    &KeyboardFocusableScrollersEnabledPolicyBrowserTest::DescribeParams);

class KeyboardFocusableScrollersDisabledPolicyBrowserTest
    : public KeyboardFocusableScrollersEnabledPolicyBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    feature_list_.Reset();
    feature_list_.InitAndDisableFeature(
        blink::features::kKeyboardFocusableScrollers);
  }
};

IN_PROC_BROWSER_TEST_P(KeyboardFocusableScrollersDisabledPolicyBrowserTest,
                       PolicyIsFollowed) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/empty.html"));
  ASSERT_TRUE(NavigateToUrl(url, this));

  content::DOMMessageQueue message_queue(
      chrome_test_utils::GetActiveWebContents(this));
  content::ExecuteScriptAsync(chrome_test_utils::GetActiveWebContents(this),
                              R"(
    const scroller = document.createElement("div");
    scroller.style = 'grey; width: 100px; height: 100px; overflow: auto';
    const content = document.createElement("div");
    content.style = 'width: 200px; height: 200px';
    scroller.appendChild(content);
    document.body.appendChild(scroller);
    scroller.focus();
    if (document.activeElement == scroller) {
      window.domAutomationController.send(true);
    } else {
      window.domAutomationController.send(false);
    }
  )");
  std::string message;
  EXPECT_TRUE(message_queue.WaitForMessage(&message));
  // Feature is disabled, expect behavior to be disabled.
  EXPECT_TRUE(message == "false");
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    KeyboardFocusableScrollersDisabledPolicyBrowserTest,
    ::testing::Values(Policy::kDefault, Policy::kTrue, Policy::kFalse),
    &KeyboardFocusableScrollersDisabledPolicyBrowserTest::DescribeParams);

}  // namespace policy
