// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/help_me_read_policy_handler.h"

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

class HelpMeReadPolicyHandlerTest : public testing::Test {
 protected:
  PolicyMap policy_;
  HelpMeReadPolicyHandler handler_;
  PrefValueMap prefs_;
};

// Only 0, 1, 2 are meaningful values in the kHelpMeReadSettings, all other
// values have no effects.
TEST_F(HelpMeReadPolicyHandlerTest, InvalidPolicy) {
  policy_.Set(key::kHelpMeReadSettings, POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(3), nullptr);
  PolicyErrorMap errors;

  handler_.ApplyPolicySettings(policy_, &prefs_);
  EXPECT_THAT(&prefs_, PrefNotSet(ash::prefs::kHmrEnabled));
  EXPECT_THAT(&prefs_, PrefNotSet(ash::prefs::kHmrFeedbackAllowed));
  EXPECT_THAT(&prefs_, PrefNotSet(ash::prefs::kMagicBoostEnabled));
}

class HelpMeReadPolicyHandlerTestP
    : public HelpMeReadPolicyHandlerTest,
      public testing::WithParamInterface<std::tuple<int, bool, bool, bool>> {
 protected:
  int policy_value() const { return std::get<0>(GetParam()); }
  bool hmr_enabled() const { return std::get<1>(GetParam()); }
  bool hmr_feedback_allowed() const { return std::get<2>(GetParam()); }
  bool magic_boost_enabled() const { return std::get<3>(GetParam()); }
};

TEST_P(HelpMeReadPolicyHandlerTestP, ValidPolicy) {
  policy_.Set(key::kHelpMeReadSettings, POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
              base::Value(policy_value()), nullptr);
  PolicyErrorMap errors;

  handler_.ApplyPolicySettings(policy_, &prefs_);
  EXPECT_THAT(&prefs_, PrefHasValue(ash::prefs::kHmrEnabled, hmr_enabled()));
  EXPECT_THAT(&prefs_, PrefHasValue(ash::prefs::kHmrFeedbackAllowed,
                                    hmr_feedback_allowed()));
  EXPECT_THAT(&prefs_, PrefHasValue(ash::prefs::kMagicBoostEnabled,
                                    magic_boost_enabled()));
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    HelpMeReadPolicyHandlerTestP,
    testing::Values(std::make_tuple(0, true, true, true),
                    std::make_tuple(1, true, false, true),
                    std::make_tuple(2, false, false, true)));

}  // namespace policy
