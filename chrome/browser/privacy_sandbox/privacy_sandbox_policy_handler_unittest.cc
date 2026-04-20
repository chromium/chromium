// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_policy_handler.h"

#include "base/strings/utf_string_conversions.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

// Tests privacy sandbox related policies and preference setting.
class PrivacySandboxPolicyHandlerTest : public testing::Test {
 public:
  PrivacySandboxPolicyHandler handler;
  policy::PolicyMap policy;
  policy::PolicyErrorMap errors;
  PrefValueMap prefs;
};



TEST_F(PrivacySandboxPolicyHandlerTest,
       testPolicyToPref_ApiDisabled_DisablesApiPref) {
  policy.Set(policy::key::kPrivacySandboxAdTopicsEnabled,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  policy.Set(policy::key::kPrivacySandboxSiteEnabledAdsEnabled,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  policy.Set(policy::key::kPrivacySandboxAdMeasurementEnabled,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);

  handler.ApplyPolicySettings(policy, &prefs);
  bool value;
  prefs.GetBoolean(prefs::kPrivacySandboxM1TopicsEnabled, &value);
  EXPECT_FALSE(value);

  prefs.GetBoolean(prefs::kPrivacySandboxM1FledgeEnabled, &value);
  EXPECT_FALSE(value);

  prefs.GetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled, &value);
  EXPECT_FALSE(value);
}

TEST_F(PrivacySandboxPolicyHandlerTest,
       testPolicyToPref_ApiEnabled_DoesNothingToApiPref) {
  policy.Set(policy::key::kPrivacySandboxAdTopicsEnabled,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
  policy.Set(policy::key::kPrivacySandboxSiteEnabledAdsEnabled,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
  policy.Set(policy::key::kPrivacySandboxAdMeasurementEnabled,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD, base::Value(true), nullptr);

  handler.ApplyPolicySettings(policy, &prefs);
  bool value;
  // Pref not found because we didn't set it.
  EXPECT_FALSE(prefs.GetBoolean(prefs::kPrivacySandboxM1TopicsEnabled, &value));

  EXPECT_FALSE(prefs.GetBoolean(prefs::kPrivacySandboxM1FledgeEnabled, &value));

  EXPECT_FALSE(
      prefs.GetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled, &value));
}
