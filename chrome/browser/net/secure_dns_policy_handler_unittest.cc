// Copyright 2019 The Chromium Authors
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
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/net/secure_dns_config.h"
#include "chrome/browser/prefs/session_startup_pref.h"
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"

namespace {

constexpr char kDohSalt[] = "test-salt";
}  // namespace

#endif

namespace policy {

class SecureDnsPolicyHandlerTest : public testing::Test {
 protected:
  void SetPolicyValue(const std::string& policy, base::Value value) {
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
  SetPolicyValue(key::kDnsOverHttpsMode, base::Value(1));

  CheckAndApplyPolicySettings();

  // Should have an error
  ASSERT_EQ(errors().size(), 1U);
  EXPECT_EQ(errors().begin()->first, key::kDnsOverHttpsMode);
}

TEST_F(SecureDnsPolicyHandlerTest, TemplatesPolicyErrorName) {
  // Do anything that causes a policy error.
  SetPolicyValue(key::kDnsOverHttpsTemplates, base::Value(1));

  CheckAndApplyPolicySettings();

  // Should have an error
  ASSERT_EQ(errors().size(), 1U);
  EXPECT_EQ(errors().begin()->first, key::kDnsOverHttpsTemplates);
}

TEST_F(SecureDnsPolicyHandlerTest, EmptyModePolicyValue) {
  SetPolicyValue(key::kDnsOverHttpsMode, base::Value(""));

  CheckAndApplyPolicySettings();

  // Should have an error
  auto expected_error =
      l10n_util::GetStringUTF16(IDS_POLICY_NOT_SPECIFIED_ERROR);
  ASSERT_EQ(errors().size(), 1U);
  EXPECT_EQ(errors().begin()->second.message, expected_error);

  // Pref should not be set.
  const base::Value* pref_value;
  EXPECT_FALSE(prefs().GetValue(prefs::kDnsOverHttpsMode, &pref_value));
}

TEST_F(SecureDnsPolicyHandlerTest, InvalidModePolicyValue) {
  SetPolicyValue(key::kDnsOverHttpsMode, base::Value("invalid"));

  CheckAndApplyPolicySettings();

  // Should have an error.
  auto expected_error =
      l10n_util::GetStringUTF16(IDS_POLICY_INVALID_SECURE_DNS_MODE_ERROR);
  EXPECT_EQ(errors().size(), 1U);
  EXPECT_EQ(errors().begin()->second.message, expected_error);

  // Pref should not be set.
  const base::Value* pref_value;
  EXPECT_FALSE(prefs().GetValue(prefs::kDnsOverHttpsMode, &pref_value));
}

TEST_F(SecureDnsPolicyHandlerTest, InvalidModePolicyType) {
  // Give an int to a string-enum policy.
  SetPolicyValue(key::kDnsOverHttpsMode, base::Value(1));

  CheckAndApplyPolicySettings();

  // Should have an error.
  auto expected_error = l10n_util::GetStringFUTF16(
      IDS_POLICY_TYPE_ERROR,
      base::ASCIIToUTF16(base::Value::GetTypeName(base::Value::Type::STRING)));
  ASSERT_EQ(errors().size(), 1U);
  EXPECT_EQ(errors().begin()->second.message, expected_error);

  // Pref should not be set.
  const base::Value* pref_value;
  EXPECT_FALSE(prefs().GetValue(prefs::kDnsOverHttpsMode, &pref_value));
}

TEST_F(SecureDnsPolicyHandlerTest, ValidModePolicyValueOff) {
  const std::string test_policy_value = SecureDnsConfig::kModeOff;

  SetPolicyValue(key::kDnsOverHttpsMode, base::Value(test_policy_value));

  CheckAndApplyPolicySettings();

  // Shouldn't error.
  EXPECT_EQ(errors().size(), 0U);

  std::string mode;
  EXPECT_TRUE(prefs().GetString(prefs::kDnsOverHttpsMode, &mode));
  // Pref should now be the test value.
  EXPECT_EQ(mode, test_policy_value);
}

TEST_F(SecureDnsPolicyHandlerTest, ValidModePolicyValueAutomatic) {
  const std::string test_policy_value = SecureDnsConfig::kModeAutomatic;

  SetPolicyValue(key::kDnsOverHttpsMode, base::Value(test_policy_value));

  CheckAndApplyPolicySettings();

  // Shouldn't error.
  EXPECT_EQ(errors().size(), 0U);

  std::string mode;
  EXPECT_TRUE(prefs().GetString(prefs::kDnsOverHttpsMode, &mode));
  // Pref should now be the test value.
  EXPECT_EQ(mode, test_policy_value);
}

TEST_F(SecureDnsPolicyHandlerTest, ValidModePolicySecure) {
  const std::string test_policy_value = SecureDnsConfig::kModeSecure;

  SetPolicyValue(key::kDnsOverHttpsMode, base::Value(test_policy_value));

  // The template policy requires a value if the mode is set to secure, so set
  // it to anything.
  SetPolicyValue(key::kDnsOverHttpsTemplates, base::Value("https://foo.test/"));

  CheckAndApplyPolicySettings();

  // Shouldn't error.
  EXPECT_EQ(errors().size(), 0U);

  std::string mode;
  EXPECT_TRUE(prefs().GetString(prefs::kDnsOverHttpsMode, &mode));
  // Pref should now be the test value.
  EXPECT_EQ(mode, test_policy_value);
}

TEST_F(SecureDnsPolicyHandlerTest, InvalidTemplatesPolicyValue) {
  // The templates policy requires a valid Mode policy or it will give an error
  // we're not testing for.
  SetPolicyValue(key::kDnsOverHttpsMode,
                 base::Value(SecureDnsConfig::kModeAutomatic));
  const std::string test_policy_value = "invalid";
  SetPolicyValue(key::kDnsOverHttpsTemplates, base::Value(test_policy_value));

  CheckAndApplyPolicySettings();

  // Should have an error
  auto expected_error =
      l10n_util::GetStringUTF16(IDS_POLICY_SECURE_DNS_TEMPLATES_INVALID_ERROR);
  EXPECT_EQ(errors().size(), 1U);
  EXPECT_EQ(errors().begin()->second.message, expected_error);

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
                 base::Value(SecureDnsConfig::kModeAutomatic));
  // Give an int to a string policy.
  SetPolicyValue(key::kDnsOverHttpsTemplates, base::Value(1));

  CheckAndApplyPolicySettings();

  // Should have an error.
  auto expected_error = l10n_util::GetStringFUTF16(
      IDS_POLICY_TYPE_ERROR,
      base::ASCIIToUTF16(base::Value::GetTypeName(base::Value::Type::STRING)));
  ASSERT_EQ(errors().size(), 1U);
  EXPECT_EQ(errors().begin()->second.message, expected_error);

  // Pref should not be set.
  const base::Value* pref_value;
  EXPECT_FALSE(prefs().GetValue(prefs::kDnsOverHttpsTemplates, &pref_value));
}

// Templates policy should error when the Mode makes its value irrelevant.
TEST_F(SecureDnsPolicyHandlerTest, IrrelevantTemplatesPolicyWithModeOff) {
  SetPolicyValue(key::kDnsOverHttpsMode,
                 base::Value(SecureDnsConfig::kModeOff));
  // Set templates to anything.
  const std::string test_policy_value = "https://foo.test/";
  SetPolicyValue(key::kDnsOverHttpsTemplates, base::Value(test_policy_value));

  CheckAndApplyPolicySettings();

  // Should have an error.
  auto expected_error = l10n_util::GetStringUTF16(
      IDS_POLICY_SECURE_DNS_TEMPLATES_IRRELEVANT_MODE_ERROR);
  ASSERT_EQ(errors().size(), 1U);
  EXPECT_EQ(errors().begin()->second.message, expected_error);

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
  SetPolicyValue(key::kDnsOverHttpsTemplates, base::Value(test_policy_value));

  CheckAndApplyPolicySettings();

  // Should have an error.
  auto expected_error = l10n_util::GetStringUTF16(
      IDS_POLICY_SECURE_DNS_TEMPLATES_UNSET_MODE_ERROR);
  ASSERT_EQ(errors().size(), 1U);
  EXPECT_EQ(errors().begin()->second.message, expected_error);

  // Pref should be set.
  std::string templates;
  EXPECT_TRUE(prefs().GetString(prefs::kDnsOverHttpsTemplates, &templates));

  // Pref should now be the test value.
  EXPECT_EQ(templates, test_policy_value);
}

TEST_F(SecureDnsPolicyHandlerTest, TemplatesWithModeInvalid) {
  // Set mode so that it's invalid.
  SetPolicyValue(key::kDnsOverHttpsMode, base::Value("foo"));
  // Set templates to anything.
  const std::string test_policy_value = "https://foo.test/";
  SetPolicyValue(key::kDnsOverHttpsTemplates, base::Value(test_policy_value));

  CheckAndApplyPolicySettings();

  // Should have errors.
  auto expected_error1 =
      l10n_util::GetStringUTF16(IDS_POLICY_INVALID_SECURE_DNS_MODE_ERROR);
  auto expected_error2 = l10n_util::GetStringUTF16(
      IDS_POLICY_SECURE_DNS_TEMPLATES_INVALID_MODE_ERROR);
  ASSERT_EQ(errors().size(), 2U);
  auto it = errors().begin();
  EXPECT_EQ(it++->second.message, expected_error1);
  EXPECT_EQ(it->second.message, expected_error2);

  // Pref should be set.
  std::string templates;
  EXPECT_TRUE(prefs().GetString(prefs::kDnsOverHttpsTemplates, &templates));

  // Pref should now be the test value.
  EXPECT_EQ(templates, test_policy_value);
}

TEST_F(SecureDnsPolicyHandlerTest, TemplatesNotSetWithModeSecure) {
  SetPolicyValue(key::kDnsOverHttpsMode,
                 base::Value(SecureDnsConfig::kModeSecure));

  CheckAndApplyPolicySettings();

  // Should have an error.
  auto expected_error = l10n_util::GetStringUTF16(
      IDS_POLICY_SECURE_DNS_TEMPLATES_NOT_SPECIFIED_ERROR);
  ASSERT_EQ(errors().size(), 1U);
  auto it = errors().begin();
  EXPECT_EQ(it->second.message, expected_error);

  // Pref should be set.
  std::string templates;
  EXPECT_TRUE(prefs().GetString(prefs::kDnsOverHttpsTemplates, &templates));

  // Templates pref should be an empty string.
  EXPECT_EQ(templates, "");
}

TEST_F(SecureDnsPolicyHandlerTest, TemplatesNotStringWithModeSecure) {
  SetPolicyValue(key::kDnsOverHttpsMode,
                 base::Value(SecureDnsConfig::kModeSecure));
  SetPolicyValue(key::kDnsOverHttpsTemplates, base::Value(1));

  CheckAndApplyPolicySettings();

  auto expected_error1 = l10n_util::GetStringFUTF16(
      IDS_POLICY_TYPE_ERROR,
      base::ASCIIToUTF16(base::Value::GetTypeName(base::Value::Type::STRING)));
  auto expected_error2 = l10n_util::GetStringUTF16(
      IDS_POLICY_SECURE_DNS_TEMPLATES_NOT_SPECIFIED_ERROR);
  EXPECT_THAT(base::SplitString(
                  errors().GetErrorMessages(key::kDnsOverHttpsTemplates), u"\n",
                  base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY),
              testing::UnorderedElementsAre(expected_error1, expected_error2));

  // Pref should be set.
  std::string templates;
  EXPECT_TRUE(prefs().GetString(prefs::kDnsOverHttpsTemplates, &templates));

  // Templates pref should be an empty string.
  EXPECT_EQ(templates, "");
}

TEST_F(SecureDnsPolicyHandlerTest, TemplatesEmptyWithModeSecure) {
  SetPolicyValue(key::kDnsOverHttpsMode,
                 base::Value(SecureDnsConfig::kModeSecure));
  SetPolicyValue(key::kDnsOverHttpsTemplates, base::Value(""));

  CheckAndApplyPolicySettings();

  // Should have an error.
  auto expected_error = l10n_util::GetStringUTF16(
      IDS_POLICY_SECURE_DNS_TEMPLATES_NOT_SPECIFIED_ERROR);
  ASSERT_EQ(errors().size(), 1U);
  auto it = errors().begin();
  EXPECT_EQ(it->second.message, expected_error);

  // Pref should be set.
  std::string templates;
  EXPECT_TRUE(prefs().GetString(prefs::kDnsOverHttpsTemplates, &templates));

  // Templates pref should be an empty string.
  EXPECT_EQ(templates, "");
}

TEST_F(SecureDnsPolicyHandlerTest, TemplatesEmptyWithModeAutomatic) {
  SetPolicyValue(key::kDnsOverHttpsMode,
                 base::Value(SecureDnsConfig::kModeAutomatic));
  SetPolicyValue(key::kDnsOverHttpsTemplates, base::Value(""));

  CheckAndApplyPolicySettings();

  // Shouldn't error.
  EXPECT_EQ(errors().size(), 0U);

  // Pref should be set.
  std::string templates;
  EXPECT_FALSE(prefs().GetString(prefs::kDnsOverHttpsTemplates, &templates));
}

TEST_F(SecureDnsPolicyHandlerTest, TemplatesPolicyWithModeAutomatic) {
  // The templates policy requires a valid Mode policy or it will give an error
  // we're not testing for.
  SetPolicyValue(key::kDnsOverHttpsMode,
                 base::Value(SecureDnsConfig::kModeAutomatic));
  const std::string test_policy_value =
      "https://foo.test/ https://bar.test/dns-query{?dns}";

  SetPolicyValue(key::kDnsOverHttpsTemplates, base::Value(test_policy_value));

  CheckAndApplyPolicySettings();

  // Shouldn't error.
  EXPECT_EQ(errors().size(), 0U);

  std::string templates;
  EXPECT_TRUE(prefs().GetString(prefs::kDnsOverHttpsTemplates, &templates));
  // Pref should now be the test value.
  EXPECT_EQ(templates, test_policy_value);
}

TEST_F(SecureDnsPolicyHandlerTest, TemplatesPolicyWithModeSecure) {
  // The templates policy requires a valid Mode policy or it will give an error
  // we're not testing for.
  SetPolicyValue(key::kDnsOverHttpsMode,
                 base::Value(SecureDnsConfig::kModeSecure));
  const std::string test_policy_value =
      "https://foo.test/ https://bar.test/dns-query{?dns}";

  SetPolicyValue(key::kDnsOverHttpsTemplates, base::Value(test_policy_value));

  CheckAndApplyPolicySettings();

  // Shouldn't error.
  EXPECT_EQ(errors().size(), 0U);

  std::string templates;
  EXPECT_TRUE(prefs().GetString(prefs::kDnsOverHttpsTemplates, &templates));
  // Pref should now be the test value.
  EXPECT_EQ(templates, test_policy_value);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(SecureDnsPolicyHandlerTest, TemplatesWithIdentifiers) {
  SetPolicyValue(key::kDnsOverHttpsMode,
                 base::Value(SecureDnsConfig::kModeSecure));
  const std::string test_policy_value = "https://foo.test/";
  SetPolicyValue(key::kDnsOverHttpsTemplatesWithIdentifiers,
                 base::Value(test_policy_value));
  SetPolicyValue(key::kDnsOverHttpsSalt, base::Value(kDohSalt));

  CheckAndApplyPolicySettings();

  EXPECT_TRUE(errors().empty());

  std::string templates, templates_with_identifiers, salt;
  EXPECT_TRUE(prefs().GetString(prefs::kDnsOverHttpsTemplatesWithIdentifiers,
                                &templates_with_identifiers));
  EXPECT_TRUE(prefs().GetString(prefs::kDnsOverHttpsSalt, &salt));
  EXPECT_TRUE(prefs().GetString(prefs::kDnsOverHttpsTemplates, &templates));
  EXPECT_EQ(templates, "");
  EXPECT_EQ(templates_with_identifiers, test_policy_value);
  EXPECT_EQ(salt, kDohSalt);
}

// Testing that setting the both the cross policy DnsOverHttpsTemplates and the
// Chrome OS policy DnsOverHttpsTemplatesWithIdentifiers result in both prefs
// being set.
TEST_F(SecureDnsPolicyHandlerTest, BothPoliciesSet) {
  SetPolicyValue(key::kDnsOverHttpsMode,
                 base::Value(SecureDnsConfig::kModeSecure));
  const std::string test_policy_value = "https://foo.test/";
  SetPolicyValue(key::kDnsOverHttpsTemplates, base::Value(test_policy_value));
  const std::string test_policy_identifiers_value =
      "https://foo.test.identifiers/";
  SetPolicyValue(key::kDnsOverHttpsTemplatesWithIdentifiers,
                 base::Value(test_policy_identifiers_value));
  SetPolicyValue(key::kDnsOverHttpsSalt, base::Value(kDohSalt));

  CheckAndApplyPolicySettings();

  EXPECT_TRUE(errors().empty());

  std::string templates, templates_with_identifiers, salt;
  EXPECT_TRUE(prefs().GetString(prefs::kDnsOverHttpsTemplatesWithIdentifiers,
                                &templates_with_identifiers));
  EXPECT_TRUE(prefs().GetString(prefs::kDnsOverHttpsSalt, &salt));
  EXPECT_TRUE(prefs().GetString(prefs::kDnsOverHttpsTemplates, &templates));
  EXPECT_EQ(templates, test_policy_value);
  EXPECT_EQ(templates_with_identifiers, test_policy_identifiers_value);
  EXPECT_EQ(salt, kDohSalt);
}

TEST_F(SecureDnsPolicyHandlerTest, TemplatesWithIdentifiersInvalid) {
  SetPolicyValue(key::kDnsOverHttpsMode,
                 base::Value(SecureDnsConfig::kModeSecure));
  SetPolicyValue(key::kDnsOverHttpsTemplatesWithIdentifiers, base::Value(1));
  SetPolicyValue(key::kDnsOverHttpsSalt, base::Value(kDohSalt));

  CheckAndApplyPolicySettings();

  EXPECT_EQ(errors().size(), 3U);
  auto expected_error1 = l10n_util::GetStringFUTF16(
      IDS_POLICY_TYPE_ERROR,
      base::ASCIIToUTF16(base::Value::GetTypeName(base::Value::Type::STRING)));
  auto expected_error2 = l10n_util::GetStringUTF16(
      IDS_POLICY_SECURE_DNS_TEMPLATES_NOT_SPECIFIED_ERROR);
  auto expected_error3 = l10n_util::GetStringFUTF16(
      IDS_POLICY_DEPENDENCY_ERROR_ANY_VALUE,
      base::UTF8ToUTF16(policy::key::kDnsOverHttpsTemplatesWithIdentifiers));
  EXPECT_EQ(
      errors().GetErrorMessages(key::kDnsOverHttpsTemplatesWithIdentifiers),
      expected_error1);
  EXPECT_EQ(errors().GetErrorMessages(key::kDnsOverHttpsTemplates),
            expected_error2);
  EXPECT_EQ(errors().GetErrorMessages(key::kDnsOverHttpsSalt), expected_error3);

  std::string templates_with_identifiers, salt;
  EXPECT_TRUE(prefs().GetString(prefs::kDnsOverHttpsTemplatesWithIdentifiers,
                                &templates_with_identifiers));
  EXPECT_TRUE(prefs().GetString(prefs::kDnsOverHttpsSalt, &salt));
  EXPECT_TRUE(templates_with_identifiers.empty());
  EXPECT_TRUE(salt.empty());
}

// The salt is optional, so this is a valid configuration.
TEST_F(SecureDnsPolicyHandlerTest, NoSalt) {
  SetPolicyValue(key::kDnsOverHttpsMode,
                 base::Value(SecureDnsConfig::kModeSecure));
  const std::string test_policy_identifiers_value =
      "https://foo.test.identifiers/";
  SetPolicyValue(key::kDnsOverHttpsTemplatesWithIdentifiers,
                 base::Value(test_policy_identifiers_value));

  CheckAndApplyPolicySettings();

  EXPECT_TRUE(errors().empty());

  std::string templates_with_identifiers, salt;
  EXPECT_TRUE(prefs().GetString(prefs::kDnsOverHttpsTemplatesWithIdentifiers,
                                &templates_with_identifiers));
  EXPECT_TRUE(prefs().GetString(prefs::kDnsOverHttpsSalt, &salt));
  EXPECT_EQ(templates_with_identifiers, test_policy_identifiers_value);
  EXPECT_EQ(salt, "");
}

TEST_F(SecureDnsPolicyHandlerTest, NoTemplatesWithIdentifiers) {
  SetPolicyValue(key::kDnsOverHttpsMode,
                 base::Value(SecureDnsConfig::kModeSecure));
  SetPolicyValue(key::kDnsOverHttpsSalt, base::Value(kDohSalt));

  CheckAndApplyPolicySettings();

  EXPECT_EQ(errors().size(), 2U);
  auto expected_error1 = l10n_util::GetStringUTF16(
      IDS_POLICY_SECURE_DNS_TEMPLATES_NOT_SPECIFIED_ERROR);
  auto expected_error2 = l10n_util::GetStringFUTF16(
      IDS_POLICY_DEPENDENCY_ERROR_ANY_VALUE,
      base::UTF8ToUTF16(policy::key::kDnsOverHttpsTemplatesWithIdentifiers));

  EXPECT_EQ(errors().GetErrorMessages(key::kDnsOverHttpsTemplates),
            expected_error1);
  EXPECT_EQ(errors().GetErrorMessages(key::kDnsOverHttpsSalt), expected_error2);

  std::string templates_with_identifiers, salt;
  EXPECT_TRUE(prefs().GetString(prefs::kDnsOverHttpsTemplatesWithIdentifiers,
                                &templates_with_identifiers));
  EXPECT_TRUE(prefs().GetString(prefs::kDnsOverHttpsSalt, &salt));

  EXPECT_TRUE(templates_with_identifiers.empty());
  EXPECT_TRUE(salt.empty());
}

TEST_F(SecureDnsPolicyHandlerTest, TemplatesWithIdentifiersEmpty) {
  SetPolicyValue(key::kDnsOverHttpsMode,
                 base::Value(SecureDnsConfig::kModeSecure));
  SetPolicyValue(key::kDnsOverHttpsTemplatesWithIdentifiers, base::Value(""));
  SetPolicyValue(key::kDnsOverHttpsSalt, base::Value(kDohSalt));

  CheckAndApplyPolicySettings();

  EXPECT_EQ(errors().size(), 2U);
  auto expected_error1 = l10n_util::GetStringUTF16(
      IDS_POLICY_SECURE_DNS_TEMPLATES_NOT_SPECIFIED_ERROR);
  auto expected_error2 = l10n_util::GetStringFUTF16(
      IDS_POLICY_DEPENDENCY_ERROR_ANY_VALUE,
      base::UTF8ToUTF16(policy::key::kDnsOverHttpsTemplatesWithIdentifiers));
  EXPECT_EQ(errors().GetErrorMessages(key::kDnsOverHttpsTemplates),
            expected_error1);
  EXPECT_EQ(errors().GetErrorMessages(key::kDnsOverHttpsSalt), expected_error2);

  std::string templates_with_identifiers, salt;
  EXPECT_TRUE(prefs().GetString(prefs::kDnsOverHttpsTemplatesWithIdentifiers,
                                &templates_with_identifiers));
  EXPECT_TRUE(prefs().GetString(prefs::kDnsOverHttpsSalt, &salt));

  EXPECT_TRUE(templates_with_identifiers.empty());
  EXPECT_TRUE(salt.empty());
}

TEST_F(SecureDnsPolicyHandlerTest, NoMode) {
  const std::string test_policy_identifiers_value =
      "https://foo.test.identifiers/";
  SetPolicyValue(key::kDnsOverHttpsTemplatesWithIdentifiers,
                 base::Value(test_policy_identifiers_value));
  SetPolicyValue(key::kDnsOverHttpsSalt, base::Value(kDohSalt));

  CheckAndApplyPolicySettings();

  EXPECT_EQ(errors().size(), 1U);
  auto expected_error = l10n_util::GetStringUTF16(
      IDS_POLICY_SECURE_DNS_TEMPLATES_UNSET_MODE_ERROR);
  EXPECT_EQ(
      errors().GetErrorMessages(key::kDnsOverHttpsTemplatesWithIdentifiers),
      expected_error);

  std::string templates_with_identifiers, salt;
  EXPECT_TRUE(prefs().GetString(prefs::kDnsOverHttpsTemplatesWithIdentifiers,
                                &templates_with_identifiers));
  EXPECT_TRUE(prefs().GetString(prefs::kDnsOverHttpsSalt, &salt));

  EXPECT_EQ(templates_with_identifiers, test_policy_identifiers_value);
  EXPECT_EQ(salt, kDohSalt);
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace policy
