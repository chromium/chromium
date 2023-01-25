// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class OutOfProcessSystemDnsResolutionEnabledTest
    : public PolicyTest,
      public testing::WithParamInterface<policy::PolicyTest::BooleanPolicy> {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    if (GetParam() != BooleanPolicy::kNotConfigured) {
      PolicyMap policies;
      policies.Set(key::kOutOfProcessSystemDnsResolutionEnabled,
                   POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                   POLICY_SOURCE_CLOUD,
                   base::Value(GetParam() == BooleanPolicy::kTrue), nullptr);
      provider_.UpdateChromePolicy(policies);
    }
  }
};

IN_PROC_BROWSER_TEST_P(OutOfProcessSystemDnsResolutionEnabledTest,
                       IsRespected) {
  // Policy always overrides the default.
  bool expected_value;
  switch (GetParam()) {
    case PolicyTest::BooleanPolicy::kTrue:
      expected_value = true;
      break;
    case PolicyTest::BooleanPolicy::kFalse:
      expected_value = false;
      break;
    case PolicyTest::BooleanPolicy::kNotConfigured:
      content::ContentBrowserClient content_client;
      expected_value =
          content_client.ShouldRunOutOfProcessSystemDnsResolution();
      break;
  }
  ChromeContentBrowserClient client;
  EXPECT_EQ(expected_value, client.ShouldRunOutOfProcessSystemDnsResolution());
}

INSTANTIATE_TEST_SUITE_P(Enabled,
                         OutOfProcessSystemDnsResolutionEnabledTest,
                         ::testing::Values(PolicyTest::BooleanPolicy::kTrue));

INSTANTIATE_TEST_SUITE_P(Disabled,
                         OutOfProcessSystemDnsResolutionEnabledTest,
                         ::testing::Values(PolicyTest::BooleanPolicy::kFalse));

INSTANTIATE_TEST_SUITE_P(
    NotSet,
    OutOfProcessSystemDnsResolutionEnabledTest,
    ::testing::Values(PolicyTest::BooleanPolicy::kNotConfigured));

}  // namespace policy
