// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/screen_capture_location_policy_handler.h"

#include <memory>
#include <string>

#include "base/values.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class ScreenCaptureLocationPolicyHandlerTest : public testing::Test {
 protected:
  PolicyMap policy_;
  ScreenCaptureLocationPolicyHandler handler_;
  PrefValueMap prefs_;
};

TEST_F(ScreenCaptureLocationPolicyHandlerTest, Default) {
  PolicyErrorMap errors;
  EXPECT_TRUE(handler_.CheckPolicySettings(policy_, &errors));
  EXPECT_EQ(0u, errors.size());
}

TEST_F(ScreenCaptureLocationPolicyHandlerTest, SetPolicyInvalid) {
  const std::string path = "/root/${google_drive}/foo";
  policy_.Set(key::kScreenCaptureLocation, POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(false),
              nullptr);
  PolicyErrorMap errors;
  EXPECT_FALSE(handler_.CheckPolicySettings(policy_, &errors));
  EXPECT_EQ(1u, errors.size());

  constexpr char16_t kExpected[] = u"Expected string value.";
  EXPECT_EQ(kExpected, errors.GetErrorMessages(key::kScreenCaptureLocation));
}

class ScreenCaptureLocationPolicyHandlerTestWithParam
    : public ScreenCaptureLocationPolicyHandlerTest,
      public testing::WithParamInterface<const char*> {};

TEST_P(ScreenCaptureLocationPolicyHandlerTestWithParam, SetPolicyValid) {
  const std::string in = GetParam();
  policy_.Set(key::kScreenCaptureLocation, POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(in), nullptr);
  PolicyErrorMap errors;
  EXPECT_TRUE(handler_.CheckPolicySettings(policy_, &errors));
  EXPECT_EQ(0u, errors.size());
}

INSTANTIATE_TEST_SUITE_P(
    ScreenCaptureLocationPolicyHandlerTestWithParamInstance,
    ScreenCaptureLocationPolicyHandlerTestWithParam,
    testing::Values("${onedrive}/foo", "${google_drive}", "", "Downloads"));

}  // namespace policy
