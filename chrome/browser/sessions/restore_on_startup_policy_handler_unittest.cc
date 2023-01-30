// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/restore_on_startup_policy_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace policy {

class RestoreOnStartupPolicyHandlerTest : public testing::Test {
 protected:
  void SetPolicyValue(const std::string& policy, base::Value value) {
    policies_.Set(policy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                  POLICY_SOURCE_CLOUD, std::move(value), nullptr);
  }
  bool CheckPolicySettings() {
    return handler_.CheckPolicySettings(policies_, &errors_);
  }
  void ApplyPolicySettings() {
    handler_.ApplyPolicySettings(policies_, &prefs_);
  }
  PolicyErrorMap& errors() { return errors_; }
  PrefValueMap& prefs() { return prefs_; }

 private:
  PolicyMap policies_;
  PolicyErrorMap errors_;
  PrefValueMap prefs_;
  RestoreOnStartupPolicyHandler handler_;
};

TEST_F(RestoreOnStartupPolicyHandlerTest, CheckPolicySettings_FailsTypeCheck) {
  // Handler expects an int; pass it a bool.
  SetPolicyValue(key::kRestoreOnStartup, base::Value(false));
  // Checking should fail and add an error to the error map.
  EXPECT_FALSE(CheckPolicySettings());
  ASSERT_EQ(1U, errors().size());
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(IDS_POLICY_TYPE_ERROR,
                                 base::ASCIIToUTF16(base::Value::GetTypeName(
                                     base::Value::Type::INTEGER))),
      errors().begin()->second.message);
}

TEST_F(RestoreOnStartupPolicyHandlerTest, CheckPolicySettings_Unspecified) {
  // Don't specify a value for the policy.
  // Checking should succeed with no errors.
  EXPECT_TRUE(CheckPolicySettings());
  EXPECT_TRUE(errors().empty());
}

TEST_F(RestoreOnStartupPolicyHandlerTest, CheckPolicySettings_UnknownValue) {
  // Specify an unknown value for the policy.
  int impossible_value = SessionStartupPref::kPrefValueLast +
                         SessionStartupPref::kPrefValueURLs +
                         SessionStartupPref::kPrefValueNewTab;
  SetPolicyValue(key::kRestoreOnStartup, base::Value(impossible_value));
  // Checking should succeed but add an error to the error map.
  EXPECT_TRUE(CheckPolicySettings());
  ASSERT_EQ(1U, errors().size());
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_POLICY_OUT_OF_RANGE_ERROR,
                base::ASCIIToUTF16(base::NumberToString(impossible_value))),
            errors().begin()->second.message);
}

TEST_F(RestoreOnStartupPolicyHandlerTest, CheckPolicySettings_HomePage) {
  // Specify the HomePage value.
  SetPolicyValue(key::kRestoreOnStartup,
                 // kPrefValueHomePage, deprecated.
                 base::Value(0));
  // Checking should succeed but add an error to the error map.
  EXPECT_TRUE(CheckPolicySettings());
  ASSERT_EQ(1U, errors().size());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_POLICY_VALUE_DEPRECATED),
            errors().begin()->second.message);
}

TEST_F(RestoreOnStartupPolicyHandlerTest,
       CheckPolicySettings_RestoreLastSession_SessionCookies) {
  // Specify the Last value and the Session-Only Cookies value.
  SetPolicyValue(key::kRestoreOnStartup,
                 base::Value(SessionStartupPref::kPrefValueLast));
  base::Value::List urls;
  urls.Append("http://foo.com");
  SetPolicyValue(key::kCookiesSessionOnlyForUrls, base::Value(std::move(urls)));
  // Checking should succeed but add an error to the error map.
  EXPECT_TRUE(CheckPolicySettings());
  ASSERT_EQ(1U, errors().size());
  EXPECT_EQ(key::kCookiesSessionOnlyForUrls, errors().begin()->first);
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(IDS_POLICY_OVERRIDDEN,
                                 base::ASCIIToUTF16(key::kRestoreOnStartup)),
      errors().begin()->second.message);
}

TEST_F(RestoreOnStartupPolicyHandlerTest, ApplyPolicySettings_NotHomePage) {
  // Specify anything except the HomePage value.
  int not_home_page = 1;  // kPrefValueHomePage + 1, deprecated.
  SetPolicyValue(key::kRestoreOnStartup, base::Value(not_home_page));
  ApplyPolicySettings();
  // The resulting prefs should have the value we specified.
  int result;
  EXPECT_TRUE(prefs().GetInteger(prefs::kRestoreOnStartup, &result));
  EXPECT_EQ(not_home_page, result);
}

TEST_F(RestoreOnStartupPolicyHandlerTest,
       CheckPolicySettings_RestoreLastSession) {
  // Specify the Last value without the conflicts.
  SetPolicyValue(key::kRestoreOnStartup,
                 base::Value(SessionStartupPref::kPrefValueLast));
  // Checking should succeed with no errors.
  EXPECT_TRUE(CheckPolicySettings());
  EXPECT_TRUE(errors().empty());
}

TEST_F(RestoreOnStartupPolicyHandlerTest, CheckPolicySettings_URLs) {
  // Specify the URLs value.
  SetPolicyValue(key::kRestoreOnStartup,
                 base::Value(SessionStartupPref::kPrefValueURLs));
  // Checking should succeed with no errors.
  EXPECT_TRUE(CheckPolicySettings());
  EXPECT_TRUE(errors().empty());
}

TEST_F(RestoreOnStartupPolicyHandlerTest,
       CheckPolicySettings_RestoreLastSessionAndURLs) {
  // Specify the LastAndURLs value.
  SetPolicyValue(key::kRestoreOnStartup,
                 base::Value(SessionStartupPref::kPrefValueLastAndURLs));
  // Checking should succeed with no errors.
  EXPECT_TRUE(CheckPolicySettings());
  EXPECT_TRUE(errors().empty());
}

TEST_F(RestoreOnStartupPolicyHandlerTest, CheckPolicySettings_NewTab) {
  // Specify the NewTab value.
  SetPolicyValue(key::kRestoreOnStartup,
                 base::Value(SessionStartupPref::kPrefValueNewTab));
  // Checking should succeed with no errors.
  EXPECT_TRUE(CheckPolicySettings());
  EXPECT_TRUE(errors().empty());
}

TEST_F(RestoreOnStartupPolicyHandlerTest, ApplyPolicySettings_NoValue) {
  // Don't specify a value for the policy.
  ApplyPolicySettings();
  // The resulting prefs should be empty.
  EXPECT_TRUE(prefs().empty());
}

TEST_F(RestoreOnStartupPolicyHandlerTest, ApplyPolicySettings_WrongType) {
  // Handler expects an int; pass it a bool.
  SetPolicyValue(key::kRestoreOnStartup, base::Value(false));
  // The resulting prefs should be empty.
  EXPECT_TRUE(prefs().empty());
}

}  // namespace policy
