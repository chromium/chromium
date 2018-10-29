// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/guest_mode_policy_handler.h"

#include "base/values.h"
#include "chrome/browser/policy/browser_signin_policy_handler.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class GuestModePolicyHandlerTest : public ::testing::Test {
 public:
  void SetUp() override {
    prefs_.Clear();
    policies_.Clear();
  }

 protected:
  void SetUpPolicy(const char* policy_name, bool value) {
    policies_.Set(policy_name, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                  POLICY_SOURCE_PLATFORM, std::make_unique<base::Value>(value),
                  nullptr);
  }

  void SetUpPolicy(const char* policy_name, int value) {
    policies_.Set(policy_name, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                  POLICY_SOURCE_PLATFORM, std::make_unique<base::Value>(value),
                  nullptr);
  }

  PolicyMap policies_;
  PrefValueMap prefs_;
  GuestModePolicyHandler handler_;
};

TEST_F(GuestModePolicyHandlerTest, ForceSigninNotSet) {
  handler_.ApplyPolicySettings(policies_, &prefs_);
  EXPECT_FALSE(prefs_.GetValue(prefs::kBrowserGuestModeEnabled, nullptr));
}

TEST_F(GuestModePolicyHandlerTest, ForceSigninDisabled) {
  SetUpPolicy(key::kForceBrowserSignin, false);
  handler_.ApplyPolicySettings(policies_, &prefs_);
  EXPECT_FALSE(prefs_.GetValue(prefs::kBrowserGuestModeEnabled, nullptr));

  SetUpPolicy(key::kForceBrowserSignin, 0);  // Invalid format
  handler_.ApplyPolicySettings(policies_, &prefs_);
  EXPECT_FALSE(prefs_.GetValue(prefs::kBrowserGuestModeEnabled, nullptr));
}

TEST_F(GuestModePolicyHandlerTest, GuestModeDisabledByDefault) {
  bool value;
  SetUpPolicy(key::kForceBrowserSignin, true);
  handler_.ApplyPolicySettings(policies_, &prefs_);
  EXPECT_TRUE(prefs_.GetBoolean(prefs::kBrowserGuestModeEnabled, &value));
  EXPECT_FALSE(value);
}

TEST_F(GuestModePolicyHandlerTest,
       GuestModeDisabledByDefaultWithInvalidFormat) {
  bool value;
  SetUpPolicy(key::kForceBrowserSignin, true);
  SetUpPolicy(key::kBrowserGuestModeEnabled, 0);
  handler_.ApplyPolicySettings(policies_, &prefs_);
  EXPECT_TRUE(prefs_.GetBoolean(prefs::kBrowserGuestModeEnabled, &value));
  EXPECT_FALSE(value);
}

TEST_F(GuestModePolicyHandlerTest, GuestModeSet) {
  bool value;
  SetUpPolicy(key::kForceBrowserSignin, true);
  SetUpPolicy(key::kBrowserGuestModeEnabled, true);
  handler_.ApplyPolicySettings(policies_, &prefs_);
  EXPECT_TRUE(prefs_.GetBoolean(prefs::kBrowserGuestModeEnabled, &value));
  EXPECT_TRUE(value);

  SetUpPolicy(key::kBrowserGuestModeEnabled, false);
  handler_.ApplyPolicySettings(policies_, &prefs_);
  EXPECT_TRUE(prefs_.GetBoolean(prefs::kBrowserGuestModeEnabled, &value));
  EXPECT_FALSE(value);
}

TEST_F(GuestModePolicyHandlerTest, GuestModeDisabledWhenBrowserSigninIsForced) {
  SetUpPolicy(key::kBrowserSignin,
              static_cast<int>(BrowserSigninMode::kForced));
  handler_.ApplyPolicySettings(policies_, &prefs_);
  bool value = true;
  EXPECT_TRUE(prefs_.GetBoolean(prefs::kBrowserGuestModeEnabled, &value));
  EXPECT_FALSE(value);
}

TEST_F(GuestModePolicyHandlerTest,
       GuestModeIsNotSetWhenBrowserSigninIsNotForced) {
  bool value = false;
  SetUpPolicy(key::kBrowserSignin,
              static_cast<int>(BrowserSigninMode::kEnabled));
  handler_.ApplyPolicySettings(policies_, &prefs_);
  EXPECT_FALSE(prefs_.GetBoolean(prefs::kBrowserGuestModeEnabled, &value));

  SetUpPolicy(key::kBrowserSignin,
              static_cast<int>(BrowserSigninMode::kDisabled));
  handler_.ApplyPolicySettings(policies_, &prefs_);
  EXPECT_FALSE(prefs_.GetBoolean(prefs::kBrowserGuestModeEnabled, &value));

  // Invalid format
  SetUpPolicy(key::kBrowserSignin, false);
  handler_.ApplyPolicySettings(policies_, &prefs_);
  EXPECT_FALSE(prefs_.GetBoolean(prefs::kBrowserGuestModeEnabled, &value));

  // Even with forceBrowserSignin enable.
  SetUpPolicy(key::kBrowserSignin,
              static_cast<int>(BrowserSigninMode::kEnabled));
  SetUpPolicy(key::kForceBrowserSignin, true);
  handler_.ApplyPolicySettings(policies_, &prefs_);
  EXPECT_FALSE(prefs_.GetBoolean(prefs::kBrowserGuestModeEnabled, &value));
}

}  // namespace policy
