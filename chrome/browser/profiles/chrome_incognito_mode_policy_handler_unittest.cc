// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/chrome_incognito_mode_policy_handler.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/policy/core/browser/configuration_policy_pref_store_test.h"
#include "components/policy/core/browser/incognito/incognito_mode_policy_handler_test.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"

namespace policy {

// Tests Incognito mode availability preference setting.
class ChromeIncognitoModePolicyHandlerTest
    : public IncognitoModePolicyHandlerTestBase {
 public:
  void SetUp() override {
    handler_list_.AddHandler(base::WrapUnique<ConfigurationPolicyHandler>(
        new ChromeIncognitoModePolicyHandler));
  }

 protected:
  enum ObsoleteIncognitoEnabledValue {
    INCOGNITO_ENABLED_UNKNOWN,
    INCOGNITO_ENABLED_TRUE,
    INCOGNITO_ENABLED_FALSE
  };

  void SetIncognitoEnabled(ObsoleteIncognitoEnabledValue incognito_enabled) {
    ASSERT_NE(incognito_enabled, INCOGNITO_ENABLED_UNKNOWN);
    policies_.Set(key::kIncognitoEnabled, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                  base::Value(incognito_enabled == INCOGNITO_ENABLED_TRUE),
                  nullptr);
  }

  void SetAvailabilityPolicies(
      ObsoleteIncognitoEnabledValue incognito_enabled,
      std::optional<policy::IncognitoModeAvailability> availability) {
    PolicyMap policy;
    if (incognito_enabled != INCOGNITO_ENABLED_UNKNOWN) {
      SetIncognitoEnabled(incognito_enabled);
    }
    if (availability.has_value()) {
      SetIncognitoModeAvailability(availability.value());
    }
    ApplyPolicies();
  }
};

// The following testcases verify that if the obsolete IncognitoEnabled
// policy is not set, the IncognitoModeAvailability values should be copied
// from IncognitoModeAvailability policy to pref "as is".
TEST_F(ChromeIncognitoModePolicyHandlerTest,
       NoObsoletePolicyAndIncognitoEnabled) {
  SetAvailabilityPolicies(INCOGNITO_ENABLED_UNKNOWN,
                          policy::IncognitoModeAvailability::kEnabled);
  VerifyAvailabilityPref(policy::IncognitoModeAvailability::kEnabled);
}

TEST_F(ChromeIncognitoModePolicyHandlerTest,
       NoObsoletePolicyAndIncognitoDisabled) {
  SetAvailabilityPolicies(INCOGNITO_ENABLED_UNKNOWN,
                          policy::IncognitoModeAvailability::kDisabled);
  VerifyAvailabilityPref(policy::IncognitoModeAvailability::kDisabled);
}

TEST_F(ChromeIncognitoModePolicyHandlerTest,
       NoObsoletePolicyAndIncognitoForced) {
  SetAvailabilityPolicies(INCOGNITO_ENABLED_UNKNOWN,
                          policy::IncognitoModeAvailability::kForced);
  VerifyAvailabilityPref(policy::IncognitoModeAvailability::kForced);
}

TEST_F(ChromeIncognitoModePolicyHandlerTest,
       NoObsoletePolicyAndNoIncognitoAvailability) {
  SetAvailabilityPolicies(INCOGNITO_ENABLED_UNKNOWN, std::nullopt);
  const base::Value* value = nullptr;
  EXPECT_FALSE(store_->GetValue(
      policy::policy_prefs::kIncognitoModeAvailability, &value));
}

// Checks that if the obsolete IncognitoEnabled policy is set, if sets
// the IncognitoModeAvailability preference only in case
// the IncognitoModeAvailability policy is not specified.
TEST_F(ChromeIncognitoModePolicyHandlerTest,
       ObsoletePolicyDoesNotAffectAvailabilityEnabled) {
  SetAvailabilityPolicies(INCOGNITO_ENABLED_FALSE,
                          policy::IncognitoModeAvailability::kEnabled);
  VerifyAvailabilityPref(policy::IncognitoModeAvailability::kEnabled);
}

TEST_F(ChromeIncognitoModePolicyHandlerTest,
       ObsoletePolicyDoesNotAffectAvailabilityForced) {
  SetAvailabilityPolicies(INCOGNITO_ENABLED_TRUE,
                          policy::IncognitoModeAvailability::kForced);
  VerifyAvailabilityPref(policy::IncognitoModeAvailability::kForced);
}

TEST_F(ChromeIncognitoModePolicyHandlerTest,
       ObsoletePolicySetsPreferenceToEnabled) {
  SetAvailabilityPolicies(INCOGNITO_ENABLED_TRUE, std::nullopt);
  VerifyAvailabilityPref(policy::IncognitoModeAvailability::kEnabled);
}

TEST_F(ChromeIncognitoModePolicyHandlerTest,
       ObsoletePolicySetsPreferenceToDisabled) {
  SetAvailabilityPolicies(INCOGNITO_ENABLED_FALSE, std::nullopt);
  VerifyAvailabilityPref(policy::IncognitoModeAvailability::kDisabled);
}

TEST_F(ChromeIncognitoModePolicyHandlerTest,
       ObsoletePolicyGetsOverridenByAllowlist) {
  SetIncognitoEnabled(INCOGNITO_ENABLED_FALSE);
  SetIncognitoModeUrlAllowlist(default_allowlist_.Clone());
  ApplyPolicies();
  VerifyAvailabilityPref(policy::IncognitoModeAvailability::kEnabled);
  VerifyBlocklistPref(base::ListValue().Append("*"));
  VerifyAllowlistPref(default_allowlist_);
}

TEST_F(ChromeIncognitoModePolicyHandlerTest,
       ObsoletePolicyGetsOverridenByAllowlistWithBlocklist) {
  SetIncognitoEnabled(INCOGNITO_ENABLED_FALSE);
  SetIncognitoModeUrlAllowlist(default_allowlist_.Clone());
  SetIncognitoModeUrlBlocklist(default_blocklist_.Clone());
  ApplyPolicies();
  VerifyAvailabilityPref(policy::IncognitoModeAvailability::kEnabled);
  VerifyBlocklistPref(base::ListValue().Append("*"));
  VerifyAllowlistPref(default_allowlist_);
}

TEST_F(ChromeIncognitoModePolicyHandlerTest,
       ObsoletePolicySetsPreferenceToDisabledWithBlocklistSet) {
  SetIncognitoEnabled(INCOGNITO_ENABLED_FALSE);
  SetIncognitoModeUrlBlocklist(default_blocklist_.Clone());
  ApplyPolicies();
  VerifyAvailabilityPref(policy::IncognitoModeAvailability::kDisabled);
}

}  // namespace policy
