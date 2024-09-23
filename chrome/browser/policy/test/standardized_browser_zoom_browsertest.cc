// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/navigation_handle.h"
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

class StandardizedBrowserZoomPolicyBrowserTest
    : public PolicyTest,
      public ::testing::WithParamInterface<Policy>,
      public content::WebContentsObserver {
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

  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override {
    navigation_handle->ForceEnableOriginTrials(
        std::vector<std::string>({"DisableStandardizedBrowserZoom"}));
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    feature_list_.InitWithFeatureStates(
        {{blink::features::kStandardizedBrowserZoom, true},
         {blink::features::kStandardizedBrowserZoomOptOut, false}});
  }

  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();

    if (GetParam() == Policy::kDefault) {
      return;
    }
    PolicyMap policies;
    SetPolicy(&policies, key::kStandardizedBrowserZoomEnabled,
              base::Value(GetParam() == Policy::kTrue));
    UpdateProviderPolicy(policies);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(StandardizedBrowserZoomPolicyBrowserTest,
                       PolicyIsFollowed) {
  // Feature should be enabled for both kDefault and kTrue.
  const bool expected_disabled = GetParam() == Policy::kFalse;

  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/empty.html"));
  ASSERT_TRUE(NavigateToUrl(url, this));
  content::EvalJsResult result =
      content::EvalJs(chrome_test_utils::GetActiveWebContents(this),
                      R"(
let l = document.body.getBoundingClientRect().left;
document.body.style.zoom = 2;
l == document.body.getBoundingClientRect().left;
)");
  ASSERT_TRUE(result.value.is_bool());
  EXPECT_EQ(expected_disabled, result.value.GetBool());
}

IN_PROC_BROWSER_TEST_P(StandardizedBrowserZoomPolicyBrowserTest,
                       OriginTrialOverridesPolicy) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/empty.html"));
  Observe(chrome_test_utils::GetActiveWebContents(this));
  ASSERT_TRUE(NavigateToUrl(url, this));
  content::EvalJsResult result =
      content::EvalJs(chrome_test_utils::GetActiveWebContents(this),
                      R"(
let l = document.body.getBoundingClientRect().left;
document.body.style.zoom = 2;
l == document.body.getBoundingClientRect().left;
)");
  ASSERT_TRUE(result.value.is_bool());
  EXPECT_TRUE(result.value.GetBool());
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    StandardizedBrowserZoomPolicyBrowserTest,
    ::testing::Values(Policy::kDefault, Policy::kTrue, Policy::kFalse),
    &StandardizedBrowserZoomPolicyBrowserTest::DescribeParams);

}  // namespace policy
