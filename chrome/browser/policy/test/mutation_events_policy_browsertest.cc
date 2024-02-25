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

class MutationEventsEnabledPolicyBrowserTest
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
    feature_list_.InitAndDisableFeature(blink::features::kMutationEvents);
  }

  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();

    if (GetParam() == Policy::kDefault) {
      return;
    }
    PolicyMap policies;
    SetPolicy(&policies, key::kMutationEventsEnabled,
              base::Value(GetParam() == Policy::kTrue));
    UpdateProviderPolicy(policies);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(MutationEventsEnabledPolicyBrowserTest,
                       PolicyIsFollowed) {
  // Both false and the default (no parameter) should be disabled.
  const bool expected_enabled = GetParam() == Policy::kTrue;

  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/empty.html"));
  ASSERT_TRUE(NavigateToUrl(url, this));

  content::DOMMessageQueue message_queue(
      chrome_test_utils::GetActiveWebContents(this));
  content::ExecuteScriptAsync(chrome_test_utils::GetActiveWebContents(this),
                              R"(
        document.addEventListener('DOMNodeRemoved',() => {
          window.domAutomationController.send(true);
        });
        // This should synchronously fire the mutation event:
        document.body.remove();
        window.domAutomationController.send(false);
      )");
  std::string message;
  EXPECT_TRUE(message_queue.WaitForMessage(&message));
  EXPECT_TRUE(message == "true" || message == "false");
  EXPECT_EQ(message == "true", expected_enabled);
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    MutationEventsEnabledPolicyBrowserTest,
    ::testing::Values(Policy::kDefault, Policy::kTrue, Policy::kFalse),
    &MutationEventsEnabledPolicyBrowserTest::DescribeParams);

}  // namespace policy
