// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/tracking_protection_policy_handler.h"

#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"
#include "testing/gtest/include/gtest/gtest.h"

class TrackingProtectionPolicyHandlerTest : public testing::Test {
 public:
  TrackingProtectionPolicyHandler handler;
  policy::PolicyMap policy;
  PrefValueMap prefs;
};

TEST_F(TrackingProtectionPolicyHandlerTest,
       IpProtectionPrefDisabledIfPolicyDisabled) {
  policy.Set(policy::key::kPrivacySandboxIpProtectionEnabled,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);

  handler.ApplyPolicySettings(policy, &prefs);

  bool value;
  ASSERT_TRUE(prefs.GetBoolean(prefs::kIpProtectionEnabled, &value));
  EXPECT_FALSE(value);
}

TEST_F(TrackingProtectionPolicyHandlerTest,
       IpProtectionPrefEnabledIfPolicyEnabled) {
  policy.Set(policy::key::kPrivacySandboxIpProtectionEnabled,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD, base::Value(true), nullptr);

  handler.ApplyPolicySettings(policy, &prefs);

  bool value;
  ASSERT_TRUE(prefs.GetBoolean(prefs::kIpProtectionEnabled, &value));
  EXPECT_TRUE(value);
}

TEST_F(TrackingProtectionPolicyHandlerTest,
       IpProtectionPrefNotAffectedIfPolicyNotSet) {
  bool value;
  EXPECT_FALSE(prefs.GetBoolean(prefs::kIpProtectionEnabled, &value));
}
