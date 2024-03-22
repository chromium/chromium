// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/multi_screen_capture_policy_handler.h"

#include <memory>

#include "base/values.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {
struct MultiScreenCaptureTestParam {
  std::vector<std::string> policy_values;
  size_t expected_number_of_errors;
};
}  // namespace

class MultiScreenCapturePolicyHandlerTest
    : public ::testing::TestWithParam<MultiScreenCaptureTestParam> {
 public:
  MultiScreenCapturePolicyHandlerTest() {
    multi_screen_capture_policy_handler_ =
        std::make_unique<MultiScreenCapturePolicyHandler>();
  }

 protected:
  std::unique_ptr<MultiScreenCapturePolicyHandler>
      multi_screen_capture_policy_handler_;
};

TEST_P(MultiScreenCapturePolicyHandlerTest, ListWithErrorsParsed) {
  const MultiScreenCaptureTestParam& test_param = GetParam();

  base::Value::List policy_values;
  for (const auto& policy_value : test_param.policy_values) {
    policy_values.Append(base::Value(policy_value));
  }

  PolicyMap policies;
  policies.Set(
      policy::key::kMultiScreenCaptureAllowedForUrls,
      PolicyLevel::POLICY_LEVEL_RECOMMENDED, PolicyScope::POLICY_SCOPE_USER,
      PolicySource::POLICY_SOURCE_ENTERPRISE_DEFAULT,
      base::Value(std::move(policy_values)), /*external_data_fetcher=*/nullptr);
  PolicyErrorMap error_map;
  EXPECT_TRUE(multi_screen_capture_policy_handler_->CheckPolicySettings(
      policies, &error_map));
  if (test_param.expected_number_of_errors == 0u) {
    EXPECT_FALSE(
        error_map.HasError(policy::key::kMultiScreenCaptureAllowedForUrls));
  } else {
    EXPECT_TRUE(
        error_map.HasError(policy::key::kMultiScreenCaptureAllowedForUrls));
    const std::vector<PolicyErrorMap::Data> errors =
        error_map.GetErrors(policy::key::kMultiScreenCaptureAllowedForUrls);
    ASSERT_EQ(errors.size(), test_param.expected_number_of_errors);
    for (const auto& error : errors) {
      EXPECT_NE(error.message.find(u"Value doesn't match format."),
                std::u16string::npos);
    }
  }
}

// This test augments the tests in MultiScreenCaptureAllowedForUrls.json to
// check if the errors are set correctly.
INSTANTIATE_TEST_SUITE_P(
    MultiScreenCapturePolicyHandler,
    MultiScreenCapturePolicyHandlerTest,
    testing::ValuesIn<MultiScreenCaptureTestParam>({
        {/*policy_values=*/{
             "isolated-app://"
             "pt2jysa7yu326m2cbu5mce4rrajvguagronrsqwn5dhbaris6eaaaaic",
             "isolated-app://"
             "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic"},
         /*expected_number_of_errors=*/0u},
        {/*policy_values=*/{
             // Invalid values:
             "https://www.chromium.org", "an invalid value",
             "isolated-app://anincompletehash",
             "isolated-app://"
             "pt2jysa7yu326m2cbu5mce4rrajvguagronrsqwn5dhbaris6eaaaaid",
             // Valid values
             "isolated-app://"
             "pt2jysa7yu326m2cbu5mce4rrajvguagronrsqwn5dhbaris6eaaaaic"},
         /*expected_number_of_errors=*/4u},
    }));

}  // namespace policy
