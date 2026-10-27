// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/browser_signin_policy_handler.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class BrowserSigninPolicyHandlerTest : public testing::Test {
 protected:
  BrowserSigninPolicyHandler handler_{Schema()};
  PolicyMap policies_;
  PolicyErrorMap errors_;
  PrefValueMap prefs_;
  base::HistogramTester histogram_tester_;
};

TEST_F(BrowserSigninPolicyHandlerTest, UserScopeDisabledLogsMetric) {
  policies_.Set(key::kBrowserSignin, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                POLICY_SOURCE_CLOUD,
                base::Value(static_cast<int>(BrowserSigninMode::kDisabled)),
                nullptr);
  handler_.ApplyPolicySettings(policies_, &prefs_);

  histogram_tester_.ExpectUniqueSample(
      "Enterprise.BrowserSignin.UserScope.PolicyValue",
      BrowserSigninMode::kDisabled, 1);
}

TEST_F(BrowserSigninPolicyHandlerTest, UserScopeEnabledLogsMetric) {
  policies_.Set(key::kBrowserSignin, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                POLICY_SOURCE_CLOUD,
                base::Value(static_cast<int>(BrowserSigninMode::kEnabled)),
                nullptr);
  handler_.ApplyPolicySettings(policies_, &prefs_);

  histogram_tester_.ExpectUniqueSample(
      "Enterprise.BrowserSignin.UserScope.PolicyValue",
      BrowserSigninMode::kEnabled, 1);
}

TEST_F(BrowserSigninPolicyHandlerTest, UserScopeForcedLogsMetric) {
  policies_.Set(key::kBrowserSignin, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                POLICY_SOURCE_CLOUD,
                base::Value(static_cast<int>(BrowserSigninMode::kForced)),
                nullptr);
  handler_.ApplyPolicySettings(policies_, &prefs_);

  histogram_tester_.ExpectUniqueSample(
      "Enterprise.BrowserSignin.UserScope.PolicyValue",
      BrowserSigninMode::kForced, 1);
}

TEST_F(BrowserSigninPolicyHandlerTest, MachineScopeDoesNotLogMetric) {
  policies_.Set(key::kBrowserSignin, POLICY_LEVEL_MANDATORY,
                POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
                base::Value(static_cast<int>(BrowserSigninMode::kDisabled)),
                nullptr);
  handler_.ApplyPolicySettings(policies_, &prefs_);

  histogram_tester_.ExpectTotalCount(
      "Enterprise.BrowserSignin.UserScope.PolicyValue", 0);
}

TEST_F(BrowserSigninPolicyHandlerTest, EmptyPolicyDoesNotLogMetric) {
  handler_.ApplyPolicySettings(policies_, &prefs_);

  histogram_tester_.ExpectTotalCount(
      "Enterprise.BrowserSignin.UserScope.PolicyValue", 0);
}

TEST_F(BrowserSigninPolicyHandlerTest,
       UserScopeRepeatedApplyDoesNotOvercountMetric) {
  policies_.Set(key::kBrowserSignin, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                POLICY_SOURCE_CLOUD,
                base::Value(static_cast<int>(BrowserSigninMode::kDisabled)),
                nullptr);
  handler_.ApplyPolicySettings(policies_, &prefs_);
  histogram_tester_.ExpectUniqueSample(
      "Enterprise.BrowserSignin.UserScope.PolicyValue",
      BrowserSigninMode::kDisabled, 1);

  // Periodic policy refresh with same value should not record again.
  handler_.ApplyPolicySettings(policies_, &prefs_);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.BrowserSignin.UserScope.PolicyValue", 1);

  // Changing the policy value should record the new value.
  policies_.Set(key::kBrowserSignin, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                POLICY_SOURCE_CLOUD,
                base::Value(static_cast<int>(BrowserSigninMode::kEnabled)),
                nullptr);
  handler_.ApplyPolicySettings(policies_, &prefs_);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.BrowserSignin.UserScope.PolicyValue", 2);
  histogram_tester_.ExpectBucketCount(
      "Enterprise.BrowserSignin.UserScope.PolicyValue",
      BrowserSigninMode::kEnabled, 1);

  // Refreshing again with kEnabled should not record again.
  handler_.ApplyPolicySettings(policies_, &prefs_);
  histogram_tester_.ExpectTotalCount(
      "Enterprise.BrowserSignin.UserScope.PolicyValue", 2);
}

}  // namespace policy
