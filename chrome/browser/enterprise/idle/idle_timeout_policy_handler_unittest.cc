// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/idle/idle_timeout_policy_handler.h"

#include <string>

#include "base/json/values_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/enterprise/idle/action.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace enterprise_idle {

using base::UTF8ToUTF16;

class IdleTimeoutPolicyHandlerTest : public testing::Test {
 protected:
  void SetPolicyValue(const std::string& policy, base::Value value) {
    policies_.Set(policy, policy::POLICY_LEVEL_MANDATORY,
                  policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_PLATFORM,
                  std::move(value), nullptr);
  }

  bool CheckPolicySettings() {
    bool results[] = {
        timeout_handler_.CheckPolicySettings(policies_, &errors_),
        actions_handler_.CheckPolicySettings(policies_, &errors_),
    };
    return base::ranges::all_of(base::span(results), std::identity{});
  }

  void ApplyPolicySettings() {
    timeout_handler_.ApplyPolicySettings(policies_, &prefs_);
    actions_handler_.ApplyPolicySettings(policies_, &prefs_);
  }

  void CheckAndApplyPolicySettings() {
    if (CheckPolicySettings())
      ApplyPolicySettings();
  }

  policy::PolicyErrorMap& errors() { return errors_; }
  PrefValueMap& prefs() { return prefs_; }

 private:
  policy::PolicyMap policies_;
  policy::PolicyErrorMap errors_;
  PrefValueMap prefs_;
  policy::Schema schema_ = policy::Schema::Wrap(policy::GetChromeSchemaData());
  IdleTimeoutPolicyHandler timeout_handler_;
  IdleTimeoutActionsPolicyHandler actions_handler_ =
      IdleTimeoutActionsPolicyHandler(schema_);
};

TEST_F(IdleTimeoutPolicyHandlerTest, PoliciesNotSet) {
  CheckAndApplyPolicySettings();

  // Shouldn't error.
  EXPECT_EQ(errors().size(), 0U);

  // Prefs should not be set.
  const base::Value* pref_value;
  EXPECT_FALSE(prefs().GetValue(prefs::kIdleTimeout, &pref_value));
  EXPECT_FALSE(prefs().GetValue(prefs::kIdleTimeoutActions, &pref_value));
}

TEST_F(IdleTimeoutPolicyHandlerTest, JustTimeout) {
  // IdleTimeout is set, but not IdleTimeoutActions.
  SetPolicyValue(policy::key::kIdleTimeout, base::Value(15));

  CheckAndApplyPolicySettings();

  // Should have an error.
  auto expected_error =
      l10n_util::GetStringFUTF16(IDS_POLICY_DEPENDENCY_ERROR_ANY_VALUE,
                                 UTF8ToUTF16(policy::key::kIdleTimeoutActions));
  EXPECT_EQ(errors().size(), 1U);
  EXPECT_EQ(errors().begin()->second.message, expected_error);

  // Prefs should not be set.
  const base::Value* pref_value;
  EXPECT_FALSE(prefs().GetValue(prefs::kIdleTimeout, &pref_value));
  EXPECT_FALSE(prefs().GetValue(prefs::kIdleTimeoutActions, &pref_value));
}

TEST_F(IdleTimeoutPolicyHandlerTest, JustActions) {
  // IdleTimeoutActions is set, but not IdleTimeout.
  SetPolicyValue(policy::key::kIdleTimeoutActions,
                 base::Value(base::Value::List()));

  CheckAndApplyPolicySettings();

  // Should have an error.
  auto expected_error =
      l10n_util::GetStringFUTF16(IDS_POLICY_DEPENDENCY_ERROR_ANY_VALUE,
                                 UTF8ToUTF16(policy::key::kIdleTimeout));
  EXPECT_EQ(errors().size(), 1U);
  EXPECT_EQ(errors().begin()->second.message, expected_error);

  // Prefs should not be set.
  const base::Value* pref_value;
  EXPECT_FALSE(prefs().GetValue(prefs::kIdleTimeout, &pref_value));
  EXPECT_FALSE(prefs().GetValue(prefs::kIdleTimeoutActions, &pref_value));
}

TEST_F(IdleTimeoutPolicyHandlerTest, InvalidTimeoutPolicyType) {
  // Give an integer to a string policy.
  SetPolicyValue(policy::key::kIdleTimeout, base::Value("invalid"));
  SetPolicyValue(policy::key::kIdleTimeoutActions,
                 base::Value(base::Value::List()));

  CheckAndApplyPolicySettings();

  // Should have an error.
  auto expected_error = l10n_util::GetStringFUTF16(
      IDS_POLICY_TYPE_ERROR,
      UTF8ToUTF16(base::Value::GetTypeName(base::Value::Type::INTEGER)));
  EXPECT_EQ(errors().size(), 1U);
  EXPECT_EQ(errors().begin()->second.message, expected_error);

  // Prefs should not be set.
  const base::Value* pref_value;
  EXPECT_FALSE(prefs().GetValue(prefs::kIdleTimeout, &pref_value));
  EXPECT_FALSE(prefs().GetValue(prefs::kIdleTimeoutActions, &pref_value));
}

TEST_F(IdleTimeoutPolicyHandlerTest, InvalidActionsPolicyType) {
  // Give a string to a string-enum policy.
  SetPolicyValue(policy::key::kIdleTimeout, base::Value(5));
  SetPolicyValue(policy::key::kIdleTimeoutActions, base::Value("invalid"));

  CheckAndApplyPolicySettings();

  // Should have an error.
  auto expected_error = l10n_util::GetStringFUTF16(
      IDS_POLICY_SCHEMA_VALIDATION_ERROR,
      u"Policy type mismatch: expected: \"list\", actual: \"string\".");
  EXPECT_EQ(errors().size(), 1U);
  EXPECT_EQ(errors().begin()->second.message, expected_error);

  // Prefs should not be set.
  const base::Value* pref_value;
  EXPECT_FALSE(prefs().GetValue(prefs::kIdleTimeout, &pref_value));
  EXPECT_FALSE(prefs().GetValue(prefs::kIdleTimeoutActions, &pref_value));
}

TEST_F(IdleTimeoutPolicyHandlerTest, InvalidActionWrongType) {
  // IdleTimeoutActions is a list, but one of the elements is not even a string.
  SetPolicyValue(policy::key::kIdleTimeout, base::Value(5));
  base::Value::List list;
  list.Append("close_browsers");
  list.Append(34);
  SetPolicyValue(policy::key::kIdleTimeoutActions,
                 base::Value(std::move(list)));

  CheckAndApplyPolicySettings();

  // Should have an error.
  auto expected_error = l10n_util::GetStringFUTF16(
      IDS_POLICY_ERROR_WITH_PATH,
      UTF8ToUTF16(policy::key::kIdleTimeoutActions) + u"[1]",
      l10n_util::GetStringFUTF16(
          IDS_POLICY_SCHEMA_VALIDATION_ERROR,
          u"Policy type mismatch: expected: \"string\", actual: \"integer\"."));
  EXPECT_EQ(errors().size(), 1U);
  EXPECT_EQ(errors().begin()->second.message, expected_error);

  // Prefs should not be set.
  const base::Value* pref_value;
  EXPECT_TRUE(prefs().GetValue(prefs::kIdleTimeout, &pref_value));
  EXPECT_TRUE(prefs().GetValue(prefs::kIdleTimeoutActions, &pref_value));
}

TEST_F(IdleTimeoutPolicyHandlerTest, ValidConfiguration) {
  SetPolicyValue(policy::key::kIdleTimeout, base::Value(15));
  base::Value::List list;
  list.Append("close_browsers");
  list.Append("show_profile_picker");
  SetPolicyValue(policy::key::kIdleTimeoutActions,
                 base::Value(std::move(list)));

  CheckAndApplyPolicySettings();

  // Should have no errors.
  EXPECT_TRUE(errors().empty());

  // Prefs should be set.
  const base::Value* pref_value;
  EXPECT_TRUE(prefs().GetValue(prefs::kIdleTimeout, &pref_value));
  ASSERT_THAT(pref_value, testing::NotNull());
  EXPECT_EQ(base::TimeDeltaToValue(base::Minutes(15)), *pref_value);

  EXPECT_TRUE(prefs().GetValue(prefs::kIdleTimeoutActions, &pref_value));
  ASSERT_THAT(pref_value, testing::NotNull());
  EXPECT_TRUE(pref_value->is_list());
  EXPECT_THAT(
      pref_value->GetList(),
      testing::ElementsAre(static_cast<int>(ActionType::kCloseBrowsers),
                           static_cast<int>(ActionType::kShowProfilePicker)));
}

TEST_F(IdleTimeoutPolicyHandlerTest, OneMinuteMinimum) {
  // Set the policy to 0, which should clamp the pref to 1.
  SetPolicyValue(policy::key::kIdleTimeout, base::Value(0));
  SetPolicyValue(policy::key::kIdleTimeoutActions,
                 base::Value(base::Value::List()));

  CheckAndApplyPolicySettings();

  // Should have an error.
  auto expected_error =
      l10n_util::GetStringFUTF16(IDS_POLICY_OUT_OF_RANGE_ERROR, u"0");
  EXPECT_EQ(errors().size(), 1U);
  EXPECT_EQ(errors().begin()->second.message, expected_error);

  // Prefs should be set.
  const base::Value* pref_value;
  EXPECT_TRUE(prefs().GetValue(prefs::kIdleTimeout, &pref_value));
  ASSERT_THAT(pref_value, testing::NotNull());
  EXPECT_EQ(base::TimeDeltaToValue(base::Minutes(1)), *pref_value);
}

TEST_F(IdleTimeoutPolicyHandlerTest, ActionNotRecognized) {
  // IdleTimeoutActions is a list, but one of the elements is not recognized
  // as a valid option. Recognized actions are applied, but not the others.
  SetPolicyValue(policy::key::kIdleTimeout, base::Value(5));
  base::Value::List list;
  list.Append("close_browsers");
  list.Append("show_profile_picker");
  list.Append("added_in_future_version_of_chrome");
  SetPolicyValue(policy::key::kIdleTimeoutActions,
                 base::Value(std::move(list)));

  CheckAndApplyPolicySettings();

  // Should have an error.
  auto expected_error = l10n_util::GetStringFUTF16(
      IDS_POLICY_ERROR_WITH_PATH,
      UTF8ToUTF16(policy::key::kIdleTimeoutActions) + u"[2]",
      l10n_util::GetStringFUTF16(IDS_POLICY_SCHEMA_VALIDATION_ERROR,
                                 u"Invalid value for string"));
  EXPECT_EQ(errors().size(), 1U);
  EXPECT_EQ(errors().begin()->second.message, expected_error);

  // Prefs should not be set.
  const base::Value* pref_value;
  EXPECT_TRUE(prefs().GetValue(prefs::kIdleTimeout, &pref_value));
  EXPECT_TRUE(prefs().GetValue(prefs::kIdleTimeoutActions, &pref_value));
  ASSERT_THAT(pref_value, testing::NotNull());
  EXPECT_TRUE(pref_value->is_list());
  EXPECT_THAT(
      pref_value->GetList(),
      testing::ElementsAre(static_cast<int>(ActionType::kCloseBrowsers),
                           static_cast<int>(ActionType::kShowProfilePicker)));
}

}  // namespace enterprise_idle
