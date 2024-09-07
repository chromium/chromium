// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/gen_ai_default_settings_policy_handler.h"

#include <string>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/policy/homepage_location_policy_handler.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

constexpr char kHelpMeWriteSettingsPrefs[] = "HelpMeWriteSettings";
constexpr char kTabOrganizerSettingsPrefs[] = "TabOrganizerSettings";
constexpr char kCreateThemesSettingsPrefs[] = "CreateThemesSettings";
constexpr char kDevToolsGenAiSettingsPrefs[] = "DevToolsGenAiSettings";
constexpr char kHistorySearchSettingsPrefs[] = "HistorySearchSettings";

constexpr int kDefaultValue = 2;

}  // namespace

class GenAiDefaultSettingsPolicyHandlerTest : public testing::Test {
 protected:
  void SetUp() override {
    std::vector<GenAiDefaultSettingsPolicyHandler::GenAiPolicyDetails>
        gen_ai_default_policies;
    gen_ai_default_policies.emplace_back(key::kHelpMeWriteSettings,
                                         kHelpMeWriteSettingsPrefs);
    gen_ai_default_policies.emplace_back(key::kTabOrganizerSettings,
                                         kTabOrganizerSettingsPrefs);
    gen_ai_default_policies.emplace_back(key::kCreateThemesSettings,
                                         kCreateThemesSettingsPrefs);
    gen_ai_default_policies.emplace_back(key::kDevToolsGenAiSettings,
                                         kDevToolsGenAiSettingsPrefs);
    gen_ai_default_policies.emplace_back(key::kHistorySearchSettings,
                                         kHistorySearchSettingsPrefs);
    handler_ = std::make_unique<GenAiDefaultSettingsPolicyHandler>(
        std::move(gen_ai_default_policies));

    policies_.Set(key::kHelpMeWriteSettings, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(1),
                  nullptr);
    policies_.Set(key::kTabOrganizerSettings, POLICY_LEVEL_RECOMMENDED,
                  POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM, base::Value(1),
                  nullptr);
    policies_.Set(key::kCreateThemesSettings, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE, POLICY_SOURCE_ENTERPRISE_DEFAULT,
                  base::Value(1), nullptr);
  }

  void SetGenAiDefaultPolicy(base::Value value) {
    policies_.Set(key::kGenAiDefaultSettings, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, std::move(value),
                  nullptr);
  }

  void ApplyPolicies() { handler_->ApplyPolicySettings(policies_, &prefs_); }

  std::unique_ptr<GenAiDefaultSettingsPolicyHandler> handler_;
  PolicyMap policies_;
  PrefValueMap prefs_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(GenAiDefaultSettingsPolicyHandlerTest, DefaultUnset) {
  ApplyPolicies();
  EXPECT_TRUE(prefs_.empty());
}

TEST_F(GenAiDefaultSettingsPolicyHandlerTest, DefaultSet) {
  SetGenAiDefaultPolicy(base::Value(kDefaultValue));
  ApplyPolicies();

  int pref_value;
  // The handler doesn't set prefs whose policies are set in `PolicyMap`.
  EXPECT_FALSE(prefs_.GetInteger(kHelpMeWriteSettingsPrefs, &pref_value));
  EXPECT_FALSE(prefs_.GetInteger(kTabOrganizerSettingsPrefs, &pref_value));
  EXPECT_FALSE(prefs_.GetInteger(kCreateThemesSettingsPrefs, &pref_value));
  // The handler sets prefs whose policies are unset in `PolicyMap`.
  EXPECT_TRUE(prefs_.GetInteger(kDevToolsGenAiSettingsPrefs, &pref_value));
  EXPECT_EQ(kDefaultValue, pref_value);
  EXPECT_TRUE(prefs_.GetInteger(kHistorySearchSettingsPrefs, &pref_value));
  EXPECT_EQ(kDefaultValue, pref_value);
}

TEST_F(GenAiDefaultSettingsPolicyHandlerTest, DefaultSetToInvalidValue) {
  SetGenAiDefaultPolicy(base::Value("value"));
  ApplyPolicies();
  EXPECT_TRUE(prefs_.empty());
}

TEST_F(GenAiDefaultSettingsPolicyHandlerTest, FeatureDisabled) {
  // Disable the feature.
  scoped_feature_list_.InitFromCommandLine(
      /* enable_features= */ "",
      /* disable_features= */ "ApplyGenAiPolicyDefaults");

  SetGenAiDefaultPolicy(base::Value(kDefaultValue));
  ApplyPolicies();
  EXPECT_TRUE(prefs_.empty());
}

}  // namespace policy
