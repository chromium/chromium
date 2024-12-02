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

namespace {

enum class Policy {
  kDefault,
  kTrue,
  kFalse,
};

}  // namespace

class SelectParserRelaxationEnabledPolicyBrowserTest
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
  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();

    if (GetParam() == Policy::kDefault) {
      return;
    }
    PolicyMap policies;
    SetPolicy(&policies, key::kSelectParserRelaxationEnabled,
              base::Value(GetParam() == Policy::kTrue));
    UpdateProviderPolicy(policies);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(SelectParserRelaxationEnabledPolicyBrowserTest,
                       // TODO(crbug.com/379724243): Re-enable this test
                       DISABLED_PolicyIsFollowed) {
  // By default the new behavior should be enabled.
  const bool expected_enabled = GetParam() != Policy::kFalse;

  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(
      embedded_test_server()->GetURL("/select_parser_relaxation.html"));
  ASSERT_TRUE(NavigateToUrl(url, this));

  const bool actual_enabled =
      content::EvalJs(chrome_test_utils::GetActiveWebContents(this),
                      "window.selectParserRelaxationEnabled")
          .ExtractBool();
  EXPECT_EQ(actual_enabled, expected_enabled);
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    SelectParserRelaxationEnabledPolicyBrowserTest,
    ::testing::Values(Policy::kDefault, Policy::kTrue, Policy::kFalse),
    &SelectParserRelaxationEnabledPolicyBrowserTest::DescribeParams);

}  // namespace policy
