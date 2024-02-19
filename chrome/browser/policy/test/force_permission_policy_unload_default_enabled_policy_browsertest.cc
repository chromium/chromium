// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "third_party/blink/public/common/features.h"

class Browser;

namespace policy {

enum class Policy {
  kDefault,
  kTrue,
  kFalse,
};

class ForcePermissionPolicyUnloadDefaultEnabledPolicyBrowserTest
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
    feature_list_.InitAndEnableFeature(blink::features::kDeprecateUnload);
  }

  void SetUpInProcessBrowserTestFixture() override {
    // Set up the policy according to the param.
    if (GetParam() == Policy::kDefault) {
      return;
    }

    PolicyTest::SetUpInProcessBrowserTestFixture();
    PolicyMap policies;
    SetPolicy(&policies, key::kForcePermissionPolicyUnloadDefaultEnabled,
              base::Value(GetParam() == Policy::kTrue));
    provider_.UpdateChromePolicy(policies);
  }

  content::RenderFrameHost* current_render_frame_host() {
    return chrome_test_utils::GetActiveWebContents(this)->GetPrimaryMainFrame();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ForcePermissionPolicyUnloadDefaultEnabledPolicyBrowserTest,
    ::testing::Values(Policy::kDefault, Policy::kTrue, Policy::kFalse),
    &ForcePermissionPolicyUnloadDefaultEnabledPolicyBrowserTest::
        DescribeParams);

// Enabled unload deprecation and the tests that unload events continue to fire
// if the enterprise policy requests that.
IN_PROC_BROWSER_TEST_P(
    ForcePermissionPolicyUnloadDefaultEnabledPolicyBrowserTest,
    PolicyIsFollowed) {
  // On Android unload will be skipped if the page can enter back/forward-cache.
  content::DisableBackForwardCacheForTesting(
      chrome_test_utils::GetActiveWebContents(this),
      content::BackForwardCache::DisableForTestingReason::
          TEST_USES_UNLOAD_EVENT);
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_2(embedded_test_server()->GetURL("a.com", "/title2.html"));

  ASSERT_TRUE(NavigateToUrl(url_1, this));

  // Set an item in localStorage and install an unload handler that will update
  // the item if it runs.
  content::RenderFrameHostWrapper rfh_1(current_render_frame_host());
  ASSERT_TRUE(content::ExecJs(rfh_1.get(), R"(
    localStorage.setItem("unload", "false");
    addEventListener("unload", () => {
        localStorage.setItem("unload", "true")
    });
  )"));

  // Navigate away. Same-origin ensures that any unload handler will complete
  // before navigation finishes.
  ASSERT_TRUE(NavigateToUrl(url_2, this));

  // Get the item's value from localStorage.
  content::RenderFrameHostWrapper rfh_2(current_render_frame_host());
  const auto& result = content::EvalJs(rfh_2.get(), R"(
    localStorage.getItem("unload");
  )");

  // The unload handler should have run if an only if the policy has disabled
  // the deprecation.
  switch (GetParam()) {
    case Policy::kTrue:
      ASSERT_EQ(result, "true");
      break;
    case Policy::kFalse:
    case Policy::kDefault:
      ASSERT_EQ(result, "false");
  }
}

}  // namespace policy
