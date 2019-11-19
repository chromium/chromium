// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/secure_dns_policy_handler.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/net/dns_util.h"
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

class SecureDnsPolicyHandlerTest : public testing::Test {
 protected:
  void SetPolicyValue(const std::string& policy,
                      std::unique_ptr<base::Value> value) {
    policies_.Set(policy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                  POLICY_SOURCE_PLATFORM, std::move(value), nullptr);
  }
  bool CheckPolicySettings() {
    return handler_.CheckPolicySettings(policies_, &errors_);
  }
  void ApplyPolicySettings() {
    handler_.ApplyPolicySettings(policies_, &prefs_);
  }

  void CheckAndApplyPolicySettings() {
    if (CheckPolicySettings())
      ApplyPolicySettings();
  }

  PolicyErrorMap& errors() { return errors_; }
  PrefValueMap& prefs() { return prefs_; }

 private:
  PolicyMap policies_;
  PolicyErrorMap errors_;
  PrefValueMap prefs_;
  SecureDnsPolicyHandler handler_;
};

TEST_F(SecureDnsPolicyHandlerTest, PoliciesNotSet) {
  CheckAndApplyPolicySettings();

  // Shouldn't error.
  EXPECT_EQ(errors().size(), 0U);

  // Prefs should not be set.
  const base::Value* pref_value;
  EXPECT_FALSE(prefs().GetValue(prefs::kDnsOverHttpsMode, &pref_value));
  EXPECT_FALSE(prefs().GetValue(prefs::kDnsOverHttpsTemplates, &pref_value));
}

// Sanity check tests to ensure the policy errors have the correct name.
TEST_F(SecureDnsPolicyHandlerTest, ModePolicyErrorName) {
  // Do anything that causes a policy error.
  SetPolicyValue(key::kDnsOverHttpsMode, std::make_unique<base::Value>(1));

  CheckAndApplyPolicySettings();

  // Should have an error
  ASSERT_EQ(errors().size(), 1U);
  EXPECT_EQ(errors().begin()->first, key::kDnsOverHttpsMode);
}

TEST_F(SecureDnsPolicyHandlerTest, TemplatesPolicyErrorName) {
  // Do anything that causes a policy error.
  SetPolicyValue(key::kDnsOverHttpsTemplates, std::make_unique<base::Value>(1));

  CheckAndApplyPolicySettings();

  // Should have an error
  ASSERT_EQ(errors().size(), 1U);
  EXPECT_EQ(errors().begin()->first, key::kDnsOverHttpsTemplates);
}

TEST_F(SecureDnsPolicyHandlerTest, EmptyModePolicyValue) {
  SetPolicyValue(key::kDnsOverHttpsMode, std::make_unique<base::Value>(""));

  CheckAndApplyPolicySettings();

  // Should have an error
  auto expected_error =
      l10n_util::GetStringUTF16(IDS_POLICY_NOT_SPECIFIED_ERROR);
  ASSERT_EQ(errors().size(), 1U);
  EXPECT_EQ(errors().begin()->second, expected_error);

  // Pref should not be set.
  const base::Value* pref_value;
  EXPECT_FALSE(prefs().GetValue(prefs::kDnsOverHttpsMode, &pref_value));
}

TEST_F(SecureDnsPolicyHandlerTest, InvalidModePolicyValue) {
  SetPolicyValue(key::kDnsOverHttpsMode,
                 std::make_unique<base::Value>("invalid"));

  CheckAndApplyPolicySettings();

  // Should have an error.
  auto expected_error =
      l10n_util::GetStringUTF16(IDS_POLICY_INVALID_SECURE_DNS_MODE_ERROR);
  EXPECT_EQ(errors().size(), 1U);
  EXPECT_EQ(errors().begin()->second, expected_error);

  // Pref should not be set.
  const base::Value* pref_value;
  EXPECT_FALSE(prefs().GetValue(prefs::kDnsOverHttpsMode, &pref_value));
}

TEST_F(SecureDnsPolicyHandlerTest, InvalidModePolicyType) {
  // Give an int to a string-enum policy.
  SetPolicyValue(key::kDnsOverHttpsMode, std::make_unique<base::Value>(1));

  CheckAndApplyPolicySettings();

  // Should have an error.
  auto expected_error = l10n_util::GetStringFUTF16(
      IDS_POLICY_TYPE_ERROR,
      base::ASCIIToUTF16(base::Value::GetTypeName(base::Value::Type::STRING)));
  ASSERT_EQ(errors().size(), 1U);
  EXPECT_EQ(errors().begin()->second, expected_error);

  // Pref should not be set.
  const base::Value* pref_value;
  EXPECT_FALSE(prefs().GetValue(prefs::kDnsOverHttpsMode, &pref_value));
}

TEST_F(SecureDnsPolicyHandlerTest, ValidModePolicyValueOff) {
  const std::string test_policy_value =
      chrome_browser_net::kDnsOverHttpsModeOff;

  SetPolicyValue(key::kDnsOverHttpsMode,
                 std::make_unique<base::Value>(test_policy_value));

  CheckAndApplyPolicySettings();

  // Shouldn't error.
  EXPECT_EQ(errors().size(), 0U);

  std::string mode;
  EXPECT_TRUE(prefs().GetString(prefs::kDnsOverHttpsMode, &mode));
  // Pref should now be the test value.
  EXPECT_EQ(mode, test_policy_value);
}

TEST_F(SecureDnsPolicyHandlerTest, ValidModePolicyValueAutomatic) {
  const std::string test_policy_value =
      chrome_browser_net::kDnsOverHttpsModeAutomatic;

  SetPolicyValue(key::kDnsOverHttpsMode,
                 std::make_unique<base::Value>(test_policy_value));

  CheckAndApplyPolicySettings();

  // Shouldn't error.
  EXPECT_EQ(errors().size(), 0U);

  std::string mode;
  EXPECT_TRUE(prefs().GetString(prefs::kDnsOverHttpsMode, &mode));
  // Pref should now be the test value.
  EXPECT_EQ(mode, test_policy_value);
}

// TODO(http://crbug.com/955454) This test should be modified once secure is a
// valid policy value for DnsOverHttpsMode.
TEST_F(SecureDnsPolicyHandlerTest, ModePolicySecureShouldError) {
  // Secure will eventually be a valid option, but for the moment it should
  // error.
  SetPolicyValue(key::kDnsOverHttpsMode,
                 std::make_unique<base::Value>(
                     chrome_browser_net::kDnsOverHttpsModeSecure));

  CheckAndApplyPolicySettings();

  // Should have errors.
  auto expected_error =
      l10n_util::GetStringUTF16(IDS_POLICY_SECURE_DNS_MODE_NOT_SUPPORTED_ERROR);
  // The templates policy will also throw an error but we're not interested in
  // it for this test. Check the total count, but only look at the error we
  // want.
  EXPECT_EQ(errors().size(), 2U);
  EXPECT_EQ(errors().begin()->second, expected_error);

  std::string mode;
  EXPECT_TRUE(prefs().GetString(prefs::kDnsOverHttpsMode, &mode));
  // Pref should have changed to "off."
  EXPECT_EQ(mode, chrome_browser_net::kDnsOverHttpsModeOff);
}

TEST_F(SecureDnsPolicyHandlerTest, InvalidTemplatesPolicyValue) {
  // The templates policy requires a valid Mode policy or it will give an error
  // we're not testing for.
  SetPolicyValue(key::kDnsOverHttpsMode,
                 std::make_unique<base::Value>(
                     chrome_browser_net::kDnsOverHttpsModeAutomatic));
  const std::string test_policy_value = "invalid";
  SetPolicyValue(key::kDnsOverHttpsTemplates,
                 std::make_unique<base::Value>(test_policy_value));

  CheckAndApplyPolicySettings();

  // Should have an error
  auto expected_error =
      l10n_util::GetStringUTF16(IDS_POLICY_SECURE_DNS_TEMPLATES_INVALID_ERROR);
  EXPECT_EQ(errors().size(), 1U);
  EXPECT_EQ(errors().begin()->second, expected_error);

  // Pref should be set.
  std::string templates;
  EXPECT_TRUE(prefs().GetString(prefs::kDnsOverHttpsTemplates, &templates));

  // Pref should now be the test value.
  EXPECT_EQ(templates, test_policy_value);
}

TEST_F(SecureDnsPolicyHandlerTest, InvalidTemplatesPolicyType) {
  // The templates policy requires a valid Mode policy or it will give an error
  // we're not testing for.
  SetPolicyValue(key::kDnsOverHttpsMode,
                 std::make_unique<base::Value>(
                     chrome_browser_net::kDnsOverHttpsModeAutomatic));
  // Give an int to a string policy.
  SetPolicyValue(key::kDnsOverHttpsTemplates, std::make_unique<base::Value>(1));

  CheckAndApplyPolicySettings();

  // Should have an error.
  auto expected_error = l10n_util::GetStringFUTF16(
      IDS_POLICY_TYPE_ERROR,
      base::ASCIIToUTF16(base::Value::GetTypeName(base::Value::Type::STRING)));
  ASSERT_EQ(errors().size(), 1U);
  EXPECT_EQ(errors().begin()->second, expected_error);

  // Pref should not be set.
  const base::Value* pref_value;
  EXPECT_FALSE(prefs().GetValue(prefs::kDnsOverHttpsTemplates, &pref_value));
}

// Templates policy should error when the Mode makes its value irrelevant.
TEST_F(SecureDnsPolicyHandlerTest, IrrelevantTemplatesPolicyWithModeOff) {
  SetPolicyValue(
      key::kDnsOverHttpsMode,
      std::make_unique<base::Value>(chrome_browser_net::kDnsOverHttpsModeOff));
  // Set templates to anything.
  const std::string test_policy_value = "https://foo.test/";
  SetPolicyValue(key::kDnsOverHttpsTemplates,
                 std::make_unique<base::Value>(test_policy_value));

  CheckAndApplyPolicySettings();

  // Should have an error.
  auto expected_error = l10n_util::GetStringUTF16(
      IDS_POLICY_SECURE_DNS_TEMPLATES_IRRELEVANT_MODE_ERROR);
  ASSERT_EQ(errors().size(), 1U);
  EXPECT_EQ(errors().begin()->second, expected_error);

  // Pref should be set.
  std::string templates;
  EXPECT_TRUE(prefs().GetString(prefs::kDnsOverHttpsTemplates, &templates));

  // Pref should now be the test value.
  EXPECT_EQ(templates, test_policy_value);
}

TEST_F(SecureDnsPolicyHandlerTest, TemplatesWithModeNotSet) {
  // Don't set mode.
  // Set templates to anything.
  const std::string test_policy_value = "https://foo.test/";
  SetPolicyValue(key::kDnsOverHttpsTemplates,
                 std::make_unique<base::Value>(test_policy_value));

  CheckAndApplyPolicySettings();

  // Should have an error.
  auto expected_error = l10n_util::GetStringUTF16(
      IDS_POLICY_SECURE_DNS_TEMPLATES_UNSET_MODE_ERROR);
  ASSERT_EQ(errors().size(), 1U);
  EXPECT_EQ(errors().begin()->second, expected_error);

  // Pref should be set.
  std::string templates;
  EXPECT_TRUE(prefs().GetString(prefs::kDnsOverHttpsTemplates, &templates));

  // Pref should now be the test value.
  EXPECT_EQ(templates, test_policy_value);
}

TEST_F(SecureDnsPolicyHandlerTest, TemplatesWithModeInvalid) {
  // Set mode so that it's invalid.
  SetPolicyValue(key::kDnsOverHttpsMode, std::make_unique<base::Value>("foo"));
  // Set templates to anything.
  const std::string test_policy_value = "https://foo.test/";
  SetPolicyValue(key::kDnsOverHttpsTemplates,
                 std::make_unique<base::Value>(test_policy_value));

  CheckAndApplyPolicySettings();

  // Should have errors.
  auto expected_error1 =
      l10n_util::GetStringUTF16(IDS_POLICY_INVALID_SECURE_DNS_MODE_ERROR);
  auto expected_error2 = l10n_util::GetStringUTF16(
      IDS_POLICY_SECURE_DNS_TEMPLATES_INVALID_MODE_ERROR);
  ASSERT_EQ(errors().size(), 2U);
  auto it = errors().begin();
  EXPECT_EQ(it++->second, expected_error1);
  EXPECT_EQ(it->second, expected_error2);

  // Pref should be set.
  std::string templates;
  EXPECT_TRUE(prefs().GetString(prefs::kDnsOverHttpsTemplates, &templates));

  // Pref should now be the test value.
  EXPECT_EQ(templates, test_policy_value);
}

// TODO(http://crbug.com/955454) These tests should be modified once secure is a
// valid policy value for DnsOverHttpsMode.
TEST_F(SecureDnsPolicyHandlerTest, TemplatesNotSetWithModeSecure) {
  SetPolicyValue(key::kDnsOverHttpsMode,
                 std::make_unique<base::Value>(
                     chrome_browser_net::kDnsOverHttpsModeSecure));

  CheckAndApplyPolicySettings();

  // Should have errors.
  // TODO(http://crbug.com/955454) Secure will eventually be a valid option, but
  // for the moment it should error.
  auto expected_error1 =
      l10n_util::GetStringUTF16(IDS_POLICY_SECURE_DNS_MODE_NOT_SUPPORTED_ERROR);
  auto expected_error2 = l10n_util::GetStringUTF16(
      IDS_POLICY_SECURE_DNS_TEMPLATES_NOT_SPECIFIED_ERROR);
  ASSERT_EQ(errors().size(), 2U);
  auto it = errors().begin();
  EXPECT_EQ(it++->second, expected_error1);
  EXPECT_EQ(it->second, expected_error2);

  // Pref should be set.
  std::string templates;
  EXPECT_TRUE(prefs().GetString(prefs::kDnsOverHttpsTemplates, &templates));

  // Templates pref should be an empty string.
  EXPECT_EQ(templates, "");
}

TEST_F(SecureDnsPolicyHandlerTest, TemplatesNotStringWithModeSecure) {
  SetPolicyValue(key::kDnsOverHttpsMode,
                 std::make_unique<base::Value>(
                     chrome_browser_net::kDnsOverHttpsModeSecure));
  SetPolicyValue(key::kDnsOverHttpsTemplates, std::make_unique<base::Value>(1));

  CheckAndApplyPolicySettings();

  // Should have errors.
  // TODO(http://crbug.com/955454) Secure will eventually be a valid option, but
  // for the moment it should error.
  auto expected_error1 =
      l10n_util::GetStringUTF16(IDS_POLICY_SECURE_DNS_MODE_NOT_SUPPORTED_ERROR);
  auto expected_error2 = l10n_util::GetStringUTF16(
      IDS_POLICY_SECURE_DNS_TEMPLATES_NOT_SPECIFIED_ERROR);
  ASSERT_EQ(errors().size(), 2U);
  auto it = errors().begin();
  EXPECT_EQ(it++->second, expected_error1);
  EXPECT_EQ(it->second, expected_error2);

  // Pref should be set.
  std::string templates;
  EXPECT_TRUE(prefs().GetString(prefs::kDnsOverHttpsTemplates, &templates));

  // Templates pref should be an empty string.
  EXPECT_EQ(templates, "");
}

TEST_F(SecureDnsPolicyHandlerTest, TemplatesEmptyWithModeSecure) {
  SetPolicyValue(key::kDnsOverHttpsMode,
                 std::make_unique<base::Value>(
                     chrome_browser_net::kDnsOverHttpsModeSecure));
  SetPolicyValue(key::kDnsOverHttpsTemplates,
                 std::make_unique<base::Value>(""));

  CheckAndApplyPolicySettings();

  // Should have errors.
  // TODO(http://crbug.com/955454) For now the value "secure" for the mode
  // policy will throw an error but we're not interested in it for this test.
  // Check the total count but only look at the error we want. Modify this test
  // when "secure" becomes valid.
  auto expected_error = l10n_util::GetStringUTF16(
      IDS_POLICY_SECURE_DNS_TEMPLATES_NOT_SPECIFIED_ERROR);
  ASSERT_EQ(errors().size(), 2U);
  auto it = errors().begin();
  ++it;
  EXPECT_EQ(it->second, expected_error);

  // Pref should be set.
  std::string templates;
  EXPECT_TRUE(prefs().GetString(prefs::kDnsOverHttpsTemplates, &templates));

  // Templates pref should be an empty string.
  EXPECT_EQ(templates, "");
}

TEST_F(SecureDnsPolicyHandlerTest, TemplatesEmptyWithModeAutomatic) {
  SetPolicyValue(key::kDnsOverHttpsMode,
                 std::make_unique<base::Value>(
                     chrome_browser_net::kDnsOverHttpsModeAutomatic));
  SetPolicyValue(key::kDnsOverHttpsTemplates,
                 std::make_unique<base::Value>(""));

  CheckAndApplyPolicySettings();

  // Shouldn't error.
  EXPECT_EQ(errors().size(), 0U);

  // Pref should be set.
  std::string templates;
  EXPECT_TRUE(prefs().GetString(prefs::kDnsOverHttpsTemplates, &templates));

  // Pref should now be the test value.
  EXPECT_EQ(templates, "");
}

TEST_F(SecureDnsPolicyHandlerTest, TemplatesPolicyWithModeAutomatic) {
  // The templates policy requires a valid Mode policy or it will give an error
  // we're not testing for.
  SetPolicyValue(key::kDnsOverHttpsMode,
                 std::make_unique<base::Value>(
                     chrome_browser_net::kDnsOverHttpsModeAutomatic));
  const std::string test_policy_value =
      "https://foo.test/ https://bar.test/dns-query{?dns}";

  SetPolicyValue(key::kDnsOverHttpsTemplates,
                 std::make_unique<base::Value>(test_policy_value));

  CheckAndApplyPolicySettings();

  // Shouldn't error.
  EXPECT_EQ(errors().size(), 0U);

  std::string templates;
  EXPECT_TRUE(prefs().GetString(prefs::kDnsOverHttpsTemplates, &templates));
  // Pref should now be the test value.
  EXPECT_EQ(templates, test_policy_value);
}

// TODO(http://crbug.com/955454) This test should be modified once secure is a
// valid policy value for DnsOverHttpsMode.
TEST_F(SecureDnsPolicyHandlerTest, TemplatesPolicyWithModeSecure) {
  // The templates policy requires a valid Mode policy or it will give an error
  // we're not testing for.
  SetPolicyValue(key::kDnsOverHttpsMode,
                 std::make_unique<base::Value>(
                     chrome_browser_net::kDnsOverHttpsModeSecure));
  const std::string test_policy_value =
      "https://foo.test/ https://bar.test/dns-query{?dns}";

  SetPolicyValue(key::kDnsOverHttpsTemplates,
                 std::make_unique<base::Value>(test_policy_value));

  CheckAndApplyPolicySettings();

  // Should have an error
  auto expected_error =
      l10n_util::GetStringUTF16(IDS_POLICY_SECURE_DNS_MODE_NOT_SUPPORTED_ERROR);
  EXPECT_EQ(errors().size(), 1U);
  EXPECT_EQ(errors().begin()->second, expected_error);

  std::string templates;
  EXPECT_TRUE(prefs().GetString(prefs::kDnsOverHttpsTemplates, &templates));
  // Pref should now be the test value.
  EXPECT_EQ(templates, test_policy_value);
}

}  // namespace policy
