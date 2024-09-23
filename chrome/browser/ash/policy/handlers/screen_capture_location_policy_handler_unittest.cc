// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/screen_capture_location_policy_handler.h"

#include <memory>
#include <string>

#include "ash/constants/ash_pref_names.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/common/chrome_features.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

MATCHER_P(PrefNotSet, name, "") {
  return !arg->GetValue(name, nullptr);
}

MATCHER_P2(PrefHasValue, name, value, "") {
  base::Value* pref_value = nullptr;
  if (arg->GetValue(name, &pref_value) && value == *pref_value) {
    return true;
  }
  *result_listener << *pref_value;
  return false;
}

}  // namespace

class ScreenCaptureLocationPolicyHandlerTest : public testing::Test {
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kSkyVault);
  }

 protected:
  PolicyMap policy_;
  ScreenCaptureLocationPolicyHandler handler_;
  PrefValueMap prefs_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ScreenCaptureLocationPolicyHandlerTest, Default) {
  PolicyErrorMap errors;
  EXPECT_TRUE(handler_.CheckPolicySettings(policy_, &errors));
  EXPECT_EQ(0u, errors.size());

  handler_.ApplyPolicySettings(policy_, &prefs_);
  EXPECT_THAT(&prefs_, PrefNotSet(ash::prefs::kCaptureModePolicySavePath));
}

TEST_F(ScreenCaptureLocationPolicyHandlerTest, SetPolicyInvalid) {
  const std::string path = "/root/${google_drive}/foo";
  policy_.Set(key::kScreenCaptureLocation, POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(path),
              nullptr);
  PolicyErrorMap errors;
  EXPECT_FALSE(handler_.CheckPolicySettings(policy_, &errors));
  EXPECT_EQ(1u, errors.size());
  constexpr char16_t kExpected[] = u"Value doesn't match format.";
  EXPECT_EQ(kExpected, errors.GetErrorMessages(key::kScreenCaptureLocation));

  handler_.ApplyPolicySettings(policy_, &prefs_);
  EXPECT_THAT(&prefs_, PrefNotSet(ash::prefs::kCaptureModePolicySavePath));
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

  handler_.ApplyPolicySettings(policy_, &prefs_);
  EXPECT_THAT(&prefs_,
              PrefHasValue(ash::prefs::kCaptureModePolicySavePath, in));
}

INSTANTIATE_TEST_SUITE_P(
    ScreenCaptureLocationPolicyHandlerTestWithParamInstance,
    ScreenCaptureLocationPolicyHandlerTestWithParam,
    testing::Values("${onedrive}/foo", "${google_drive}", "", "Downloads"));

TEST_F(ScreenCaptureLocationPolicyHandlerTest, FeatureDisabled) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndDisableFeature(features::kSkyVault);
  const std::string path = "${google_drive}";
  policy_.Set(key::kScreenCaptureLocation, POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(path),
              nullptr);
  PolicyErrorMap errors;
  EXPECT_TRUE(handler_.CheckPolicySettings(policy_, &errors));
  EXPECT_EQ(0u, errors.size());

  handler_.ApplyPolicySettings(policy_, &prefs_);
  EXPECT_THAT(&prefs_, PrefNotSet(ash::prefs::kCaptureModePolicySavePath));
}

}  // namespace policy
