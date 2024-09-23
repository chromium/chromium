// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/annotations/blocklist_handler.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_features.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Eq;

namespace policy {

const char kAutofillQueryPolicy[] = "PasswordManagerEnabled";

class NetworkAnnotationBlocklistHandlerTest : public testing::Test {
 protected:
  base::Value::Dict blocklist_prefs() {
    return prefs_.AsDict()
        .FindDict(prefs::kNetworkAnnotationBlocklist)
        ->Clone();
  }

  // Helper function to create a simple boolean policy with given value.
  PolicyMap::Entry BooleanPolicy(bool value) {
    PolicyMap::Entry policy;
    policy.set_value(base::Value(value));
    return policy;
  }

  NetworkAnnotationBlocklistHandler handler_;
  PrefValueMap prefs_;
  PolicyMap policies_;
};

TEST_F(NetworkAnnotationBlocklistHandlerTest, DisabledByDefault) {
  EXPECT_FALSE(handler_.CheckPolicySettings(PolicyMap(), nullptr));
}

TEST_F(NetworkAnnotationBlocklistHandlerTest, EnabledWithFeature) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kNetworkAnnotationMonitoring);

  EXPECT_TRUE(handler_.CheckPolicySettings(PolicyMap(), nullptr));
}

TEST_F(NetworkAnnotationBlocklistHandlerTest, ApplyPolicySettings) {
  // Unrelated policy should not set pref.
  policies_.Set("FooPolicy", BooleanPolicy(false));
  handler_.ApplyPolicySettings(policies_, &prefs_);
  EXPECT_THAT(blocklist_prefs().size(), Eq(0));

  // Policy should not set pref if enabled.
  policies_.Set(kAutofillQueryPolicy, BooleanPolicy(true));
  handler_.ApplyPolicySettings(policies_, &prefs_);
  EXPECT_THAT(blocklist_prefs().size(), Eq(0));

  // Policy should set pref if disabled.
  policies_.Set(kAutofillQueryPolicy, BooleanPolicy(false));
  handler_.ApplyPolicySettings(policies_, &prefs_);
  EXPECT_THAT(blocklist_prefs().size(), Eq(1));

  // Policy should not set pref if policy value unset. Also verifies the pref
  // can be unset.
  policies_.Set(kAutofillQueryPolicy, PolicyMap::Entry());
  handler_.ApplyPolicySettings(policies_, &prefs_);
  EXPECT_THAT(blocklist_prefs().size(), Eq(0));
}

TEST_F(NetworkAnnotationBlocklistHandlerTest, RegisterPref) {
  TestingPrefServiceSimple pref_service_;
  EXPECT_FALSE(
      pref_service_.FindPreference(prefs::kNetworkAnnotationBlocklist));
  handler_.RegisterPrefs(pref_service_.registry());
  EXPECT_TRUE(pref_service_.FindPreference(prefs::kNetworkAnnotationBlocklist));
}

}  // namespace policy
