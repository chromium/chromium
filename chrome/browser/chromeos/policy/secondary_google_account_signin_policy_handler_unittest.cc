// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
#include "chrome/browser/chromeos/policy/secondary_google_account_signin_policy_handler.h"
#include "chromeos/constants/chromeos_pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class SecondaryGoogleAccountSigninPolicyHandlerTest : public testing::Test {
 protected:
  SecondaryGoogleAccountSigninPolicyHandlerTest() = default;

  void SetPolicy(std::unique_ptr<base::Value> value) {
    policies_.Set(key::kSecondaryGoogleAccountSigninAllowed,
                  POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                  POLICY_SOURCE_CLOUD, std::move(value),
                  nullptr /* external_data_fetcher */);
  }

  void ApplyPolicySettings(bool value) {
    SetPolicy(std::make_unique<base::Value>(value));
    handler_.ApplyPolicySettings(policies_, &prefs_);
  }

  bool GetMirrorAccountConsistencyPref() {
    bool pref = false;
    bool success =
        prefs_.GetBoolean(prefs::kAccountConsistencyMirrorRequired, &pref);
    EXPECT_TRUE(success);
    return pref;
  }

  void SetAccountConsistencyPref(bool pref) {
    prefs_.SetBoolean(prefs::kAccountConsistencyMirrorRequired, pref);
  }

  bool GetSecondaryGoogleAccountSigninAllowedPref() {
    bool pref = false;
    bool success = prefs_.GetBoolean(
        chromeos::prefs::kSecondaryGoogleAccountSigninAllowed, &pref);
    EXPECT_TRUE(success);
    return pref;
  }

  void SetSecondaryGoogleAccountSigninPref(bool pref) {
    prefs_.SetBoolean(chromeos::prefs::kSecondaryGoogleAccountSigninAllowed,
                      pref);
  }

 private:
  SecondaryGoogleAccountSigninPolicyHandler handler_;
  PolicyMap policies_;
  PrefValueMap prefs_;

  DISALLOW_COPY_AND_ASSIGN(SecondaryGoogleAccountSigninPolicyHandlerTest);
};

// If the policy is set to true, it should not override the default pref values
// (set to true in this test case).
TEST_F(SecondaryGoogleAccountSigninPolicyHandlerTest,
       SettingPolicyToTrueDoesNotChangeDefaultPreferencesSetToTrue) {
  // Set prefs to |true|.
  SetAccountConsistencyPref(true);
  SetSecondaryGoogleAccountSigninPref(true);
  // Set policy to |true|.
  ApplyPolicySettings(true /* policy value */);

  // Test that the prefs should be set to |true|.
  EXPECT_TRUE(GetMirrorAccountConsistencyPref());
  EXPECT_TRUE(GetSecondaryGoogleAccountSigninAllowedPref());
}

// If the policy is set to true, it should not override the default pref values
// (set to false in this test case).
TEST_F(SecondaryGoogleAccountSigninPolicyHandlerTest,
       SettingPolicyToTrueDoesNotChangeDefaultPreferencesSetToFalse) {
  // Set prefs to |false|.
  SetAccountConsistencyPref(false);
  SetSecondaryGoogleAccountSigninPref(false);
  // Set policy to |true|.
  ApplyPolicySettings(true /* policy value */);

  // Test that the prefs should be set to |false|.
  EXPECT_FALSE(GetMirrorAccountConsistencyPref());
  EXPECT_FALSE(GetSecondaryGoogleAccountSigninAllowedPref());
}

TEST_F(SecondaryGoogleAccountSigninPolicyHandlerTest,
       SettingPolicyToFalseEnablesMirror) {
  SetAccountConsistencyPref(false);
  ApplyPolicySettings(false /* policy value */);

  EXPECT_TRUE(GetMirrorAccountConsistencyPref());
}

TEST_F(SecondaryGoogleAccountSigninPolicyHandlerTest,
       SettingPolicyToFalseDisablesSecondaryAccountSignins) {
  SetSecondaryGoogleAccountSigninPref(true);
  ApplyPolicySettings(false /* policy value */);

  EXPECT_FALSE(GetSecondaryGoogleAccountSigninAllowedPref());
}

}  // namespace policy
