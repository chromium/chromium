// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_management_constants.h"
#include "chrome/browser/extensions/external_policy_loader.h"
#include "chrome/browser/extensions/policy_handlers.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"
#include "extensions/browser/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/win_util.h"
#endif

namespace extensions {

const char kTestPref[] = "unit_test.test_pref";

const char kTestManagementPolicy1[] =
    "{"
    "  \"abcdefghijklmnopabcdefghijklmnop\": {"
    "    \"installation_mode\": \"force_installed\","
    "  },"
    "}";

const char kTestManagementPolicy2[] =
    "{"
    "  \"abcdefghijklmnopabcdefghijklmnop\": {"
    "    \"installation_mode\": \"force_installed\","
    "    \"update_url\": \"http://example.com/app\","
    "  },"
    "  \"*\": {"
    "    \"installation_mode\": \"blocked\","
    "  },"
    "}";

const char kSensitiveTestManagementPolicy[] = R"({
  "[BLOCKED]abcdefghijklmnopabcdefghijklmnop": {
    "installation_mode": "force_installed",
    "update_url": "https://example.com/app",
  },
  "[BLOCKED]abcdefghijklmnopabcdefghijklmnpo,
    abcdefghijklmnopabcdefghijklmopn": {
      "installation_mode": "normal_installed",
      "update_url": "https://example.com/app",
  },
  "*": {
    "installation_mode": "blocked",
  },
})";

const char kSanitizedTestManagementPolicy[] =
    "{"
    "  \"*\": {"
    "    \"installation_mode\": \"blocked\","
    "  },"
    "}";

const char kTestManagementPolicy3[] =
    "{"
    "  \"*\": {"
    "    \"runtime_blocked_hosts\": [\"%s\"]"
    "  }"
    "}";

const char kTestManagementPolicy4[] =
    "{"
    "  \"*\": {"
    "    \"runtime_allowed_hosts\": [\"%s\"]"
    "  }"
    "}";

const char kTestManagementPolicy5[] =
    "{"
    "  \"*\": {"
    "    \"runtime_allowed_hosts\": [\"invalid string\"]"
    "  },"
    "  \"abcdefghijklmnopabcdefghijklmnop\": {"
    "    \"toolbar_pin\": \"force_pinned\""
    "  },"
    "}";
const char kSanitizedTestManagementPolicy5[] =
    "{"
    "  \"abcdefghijklmnopabcdefghijklmnop\": {"
    "    \"toolbar_pin\": \"force_pinned\""
    "  },"
    "}";

constexpr int kJsonParseOptions =
    base::JSON_PARSE_CHROMIUM_EXTENSIONS | base::JSON_ALLOW_TRAILING_COMMAS;

TEST(ExtensionListPolicyHandlerTest, CheckPolicySettings) {
  base::Value::List list;
  policy::PolicyMap policy_map;
  policy::PolicyErrorMap errors;
  ExtensionListPolicyHandler handler(policy::key::kExtensionInstallBlocklist,
                                     kTestPref, true);

  policy_map.Set(policy::key::kExtensionInstallBlocklist,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(list.Clone()),
                 nullptr);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  list.Append("abcdefghijklmnopabcdefghijklmnop");
  policy_map.Set(policy::key::kExtensionInstallBlocklist,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(list.Clone()),
                 nullptr);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  list.Append("*");
  policy_map.Set(policy::key::kExtensionInstallBlocklist,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(list.Clone()),
                 nullptr);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  list.Append("invalid");
  policy_map.Set(policy::key::kExtensionInstallBlocklist,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(list.Clone()),
                 nullptr);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());
  EXPECT_FALSE(
      errors.GetErrorMessages(policy::key::kExtensionInstallBlocklist).empty());
}

TEST(ExtensionSettingsPolicyHandlerTest, CheckPolicySettingsURL) {
  std::vector<std::string> good_urls = {"*://*.example.com", "*://example.com",
                                        "http://cat.example.com", "<all_urls>"};

  // Invalid URLPattern or with a non-standard path
  std::vector<std::string> bad_urls = {
      "://*.example.com",       "*://example.com/cat*",  "*://example.com/",
      "*://*.example.com/*cat", "*://example.com/cat/*", "bad",
      "*://example.com/*",      "https://example.*",     "*://*.example.*"};

  // Crafts and parses a ExtensionSettings policy to test URL parsing.
  auto url_parses_successfully = [](const char* policy_template,
                                    const std::string& url) {
    std::string policy = base::StringPrintf(policy_template, url.c_str());
    absl::optional<base::Value> policy_value =
        base::JSONReader::Read(policy, kJsonParseOptions);
    if (!policy_value)
      return false;

    policy::Schema chrome_schema =
        policy::Schema::Wrap(policy::GetChromeSchemaData());
    policy::PolicyMap policy_map;
    ExtensionSettingsPolicyHandler handler(chrome_schema);

    policy_map.Set(policy::key::kExtensionSettings,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD, std::move(*policy_value),
                   nullptr);

    policy::PolicyErrorMap errors;
    return handler.CheckPolicySettings(policy_map, &errors) && errors.empty();
  };

  for (const std::string& url : good_urls) {
    EXPECT_TRUE(url_parses_successfully(kTestManagementPolicy3, url)) << url;
    EXPECT_TRUE(url_parses_successfully(kTestManagementPolicy4, url)) << url;
  }

  for (const std::string& url : bad_urls) {
    EXPECT_FALSE(url_parses_successfully(kTestManagementPolicy3, url)) << url;
    EXPECT_FALSE(url_parses_successfully(kTestManagementPolicy4, url)) << url;
  }
}

TEST(ExtensionListPolicyHandlerTest, ApplyPolicySettings) {
  base::Value::List policy;
  base::Value::List expected;
  policy::PolicyMap policy_map;
  PrefValueMap prefs;
  base::Value* value = nullptr;
  ExtensionListPolicyHandler handler(policy::key::kExtensionInstallBlocklist,
                                     kTestPref, false);

  policy.Append("abcdefghijklmnopabcdefghijklmnop");
  expected.Append("abcdefghijklmnopabcdefghijklmnop");

  policy_map.Set(policy::key::kExtensionInstallBlocklist,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(policy.Clone()),
                 nullptr);
  handler.ApplyPolicySettings(policy_map, &prefs);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_EQ(expected, value->GetList());

  policy.Append("invalid");
  policy_map.Set(policy::key::kExtensionInstallBlocklist,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(policy.Clone()),
                 nullptr);
  handler.ApplyPolicySettings(policy_map, &prefs);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_EQ(expected, value->GetList());
}

TEST(ExtensionInstallForceListPolicyHandlerTest, CheckPolicySettings) {
  base::Value::List list;
  policy::PolicyMap policy_map;
  policy::PolicyErrorMap errors;
  ExtensionInstallForceListPolicyHandler handler;

  // Start with an empty policy.
  policy_map.Set(policy::key::kExtensionInstallForcelist,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(list.Clone()),
                 nullptr);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  // Add a correct entry. No errors should be generated.
  list.Append("abcdefghijklmnopabcdefghijklmnop;http://example.com");
  policy_map.Set(policy::key::kExtensionInstallForcelist,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(list.Clone()),
                 nullptr);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  // Add an erroneous entry. This should generate an error, but the good
  // entry should still be translated successfully.
  list.Append("adfasdf;http://example.com");
  policy_map.Set(policy::key::kExtensionInstallForcelist,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(list.Clone()),
                 nullptr);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_EQ(1U, errors.size());

  // Add an entry with bad URL, which should generate another error.
  list.Append("abcdefghijklmnopabcdefghijklmnop;nourl");
  policy_map.Set(policy::key::kExtensionInstallForcelist,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(list.Clone()),
                 nullptr);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_EQ(2U, errors.size());

  // Just an extension ID should be accepted.
  list.Append("abcdefghijklmnopabcdefghijklmnop");
  policy_map.Set(policy::key::kExtensionInstallForcelist,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(list.Clone()),
                 nullptr);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_EQ(2U, errors.size());
}

TEST(ExtensionInstallForceListPolicyHandlerTest, ApplyPolicySettings) {
  base::Value::List policy;
  base::Value::Dict expected;
  policy::PolicyMap policy_map;
  PrefValueMap prefs;
  base::Value* value = nullptr;
  ExtensionInstallForceListPolicyHandler handler;

  // Start with the policy being missing. This shouldn't affect the pref.
  handler.ApplyPolicySettings(policy_map, &prefs);
  EXPECT_FALSE(prefs.GetValue(pref_names::kInstallForceList, &value));
  EXPECT_FALSE(value);
  EXPECT_EQ(base::Value::Dict(), handler.GetPolicyDict(policy_map));

  // Set the policy to an empty value. This shouldn't affect the pref.
  policy_map.Set(policy::key::kExtensionInstallForcelist,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(policy.Clone()),
                 nullptr);
  handler.ApplyPolicySettings(policy_map, &prefs);
  EXPECT_TRUE(prefs.GetValue(pref_names::kInstallForceList, &value));
  EXPECT_EQ(expected, *value);
  EXPECT_EQ(expected, handler.GetPolicyDict(policy_map));

  // Add a correct entry to the policy. The pref should contain a corresponding
  // entry.
  policy.Append("abcdefghijklmnopabcdefghijklmnop;http://example.com");
  extensions::ExternalPolicyLoader::AddExtension(
      expected, "abcdefghijklmnopabcdefghijklmnop", "http://example.com");
  policy_map.Set(policy::key::kExtensionInstallForcelist,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(policy.Clone()),
                 nullptr);
  handler.ApplyPolicySettings(policy_map, &prefs);
  EXPECT_TRUE(prefs.GetValue(pref_names::kInstallForceList, &value));
  EXPECT_EQ(expected, *value);
  EXPECT_EQ(expected, handler.GetPolicyDict(policy_map));

  // Add a correct entry with an omitted update URL. The pref should contain now
  // two entries, with the default update URL substituted for the new entry.
  // Note: the URL hardcoded below is part of the public policy contract (as
  // documented in the policy_templates.json file), and therefore any changes to
  // it must be carefully thought out.
  policy.Append("bcdefghijklmnopabcdefghijklmnopa");
  extensions::ExternalPolicyLoader::AddExtension(
      expected, "bcdefghijklmnopabcdefghijklmnopa",
      "https://clients2.google.com/service/update2/crx");
  policy_map.Set(policy::key::kExtensionInstallForcelist,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(policy.Clone()),
                 nullptr);
  handler.ApplyPolicySettings(policy_map, &prefs);
  EXPECT_TRUE(prefs.GetValue(pref_names::kInstallForceList, &value));
  EXPECT_EQ(expected, *value);
  EXPECT_EQ(expected, handler.GetPolicyDict(policy_map));

  // Add an invalid entry. The pref should still contain two previous entries.
  policy.Append("invalid");
  policy_map.Set(policy::key::kExtensionInstallForcelist,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(policy.Clone()),
                 nullptr);
  handler.ApplyPolicySettings(policy_map, &prefs);
  EXPECT_TRUE(prefs.GetValue(pref_names::kInstallForceList, &value));
  EXPECT_EQ(expected, *value);
  EXPECT_EQ(expected, handler.GetPolicyDict(policy_map));
}

TEST(ExtensionURLPatternListPolicyHandlerTest, CheckPolicySettings) {
  base::Value::List list;
  policy::PolicyMap policy_map;
  policy::PolicyErrorMap errors;
  ExtensionURLPatternListPolicyHandler handler(
      policy::key::kExtensionInstallSources, kTestPref);

  policy_map.Set(policy::key::kExtensionInstallSources,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(list.Clone()),
                 nullptr);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  list.Append("http://*.google.com/*");
  policy_map.Set(policy::key::kExtensionInstallSources,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(list.Clone()),
                 nullptr);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  list.Append("<all_urls>");
  policy_map.Set(policy::key::kExtensionInstallSources,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(list.Clone()),
                 nullptr);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  list.Append("invalid");
  policy_map.Set(policy::key::kExtensionInstallSources,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(list.Clone()),
                 nullptr);
  errors.Clear();
  EXPECT_FALSE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());
  EXPECT_FALSE(
      errors.GetErrorMessages(policy::key::kExtensionInstallSources).empty());

  // URLPattern syntax has a different way to express 'all urls'. Though '*'
  // would be compatible today, it would be brittle, so we disallow.
  list.Append("*");
  policy_map.Set(policy::key::kExtensionInstallSources,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(list.Clone()),
                 nullptr);
  errors.Clear();
  EXPECT_FALSE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());
  EXPECT_FALSE(
      errors.GetErrorMessages(policy::key::kExtensionInstallSources).empty());
}

TEST(ExtensionURLPatternListPolicyHandlerTest, ApplyPolicySettings) {
  base::Value::List list;
  policy::PolicyMap policy_map;
  PrefValueMap prefs;
  base::Value* value = nullptr;
  ExtensionURLPatternListPolicyHandler handler(
      policy::key::kExtensionInstallSources, kTestPref);

  list.Append("https://corp.monkey.net/*");
  policy_map.Set(policy::key::kExtensionInstallSources,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(list.Clone()),
                 nullptr);
  handler.ApplyPolicySettings(policy_map, &prefs);
  ASSERT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_EQ(list, *value);
}

TEST(ExtensionSettingsPolicyHandlerTest, CheckPolicySettings) {
  auto policy_result = base::JSONReader::ReadAndReturnValueWithError(
      kTestManagementPolicy1, kJsonParseOptions);
  ASSERT_TRUE(policy_result.has_value()) << policy_result.error().message;

  policy::Schema chrome_schema =
      policy::Schema::Wrap(policy::GetChromeSchemaData());
  policy::PolicyMap policy_map;
  policy::PolicyErrorMap errors;
  PrefValueMap prefs;
  ExtensionSettingsPolicyHandler handler(chrome_schema);

  policy_map.Set(policy::key::kExtensionSettings,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, std::move(*policy_result),
                 nullptr);
  // CheckPolicySettings() has an error message because of the missing update
  // URL.
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());

  // ApplyPolicySettings() gives an empty policy.
  handler.ApplyPolicySettings(policy_map, &prefs);
  base::Value* value = nullptr;
  ASSERT_TRUE(prefs.GetValue(pref_names::kExtensionManagement, &value));
  base::Value::Dict empty_value;
  EXPECT_EQ(empty_value, *value);
}

TEST(ExtensionSettingsPolicyHandlerTest, CheckPolicySettingsTooManyHosts) {
  const char policy_template[] =
      "{"
      "  \"*\": {"
      "    \"runtime_blocked_hosts\": [%s],"
      "    \"runtime_allowed_hosts\": [%s]"
      "  }"
      "}";

  std::string urls;
  for (size_t i = 0; i < 101; ++i)
    urls.append("\"*://example" + base::NumberToString(i) + ".com\",");

  std::string policy =
      base::StringPrintf(policy_template, urls.c_str(), urls.c_str());

  std::string error;
  auto policy_value =
      base::JSONReader::ReadAndReturnValueWithError(policy, kJsonParseOptions);
  policy::Schema chrome_schema =
      policy::Schema::Wrap(policy::GetChromeSchemaData());
  policy::PolicyMap policy_map;
  policy::PolicyErrorMap errors;
  ExtensionSettingsPolicyHandler handler(chrome_schema);

  policy_map.Set(policy::key::kExtensionSettings,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, policy_value->Clone(), nullptr);

  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_EQ(2u, errors.size());
  auto error_str = errors.GetErrorMessages(policy::key::kExtensionSettings);
  auto expected_allowed = l10n_util::GetStringFUTF16(
      IDS_POLICY_EXTENSION_SETTINGS_ORIGIN_LIMIT_WARNING,
      base::NumberToString16(schema_constants::kMaxItemsURLPatternSet));
  auto expected_blocked = l10n_util::GetStringFUTF16(
      IDS_POLICY_EXTENSION_SETTINGS_ORIGIN_LIMIT_WARNING,
      base::NumberToString16(schema_constants::kMaxItemsURLPatternSet));

  EXPECT_TRUE(error_str.find(expected_allowed) != std::wstring::npos);
  EXPECT_TRUE(error_str.find(expected_blocked) != std::wstring::npos);
}

TEST(ExtensionSettingsPolicyHandlerTest, ApplyPolicySettings) {
  // Mark as enterprise managed.
  auto policy_result = base::JSONReader::ReadAndReturnValueWithError(
      kTestManagementPolicy2, kJsonParseOptions);
  ASSERT_TRUE(policy_result.has_value()) << policy_result.error().message;

  policy::Schema chrome_schema =
      policy::Schema::Wrap(policy::GetChromeSchemaData());
  policy::PolicyMap policy_map;
  policy::PolicyErrorMap errors;
  PrefValueMap prefs;
  ExtensionSettingsPolicyHandler handler(chrome_schema);

  policy_map.Set(policy::key::kExtensionSettings,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, policy_result->Clone(), nullptr);
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  handler.ApplyPolicySettings(policy_map, &prefs);
  base::Value* value = nullptr;
  ASSERT_TRUE(prefs.GetValue(pref_names::kExtensionManagement, &value));
  EXPECT_EQ(*policy_result, *value);
}

TEST(ExtensionSettingsPolicyHandlerTest, DropInvalidKeys) {
  // Check that invalid keys are dropped from the dictionary, but the rest of
  // the settings apply correctly.

  auto policy_result = base::JSONReader::ReadAndReturnValueWithError(
      kTestManagementPolicy5, kJsonParseOptions);
  ASSERT_TRUE(policy_result.has_value()) << policy_result.error().message;

  auto stripped_policy_result = base::JSONReader::ReadAndReturnValueWithError(
      kSanitizedTestManagementPolicy5, kJsonParseOptions);
  ASSERT_TRUE(stripped_policy_result.has_value())
      << stripped_policy_result.error().message;

  policy::Schema chrome_schema =
      policy::Schema::Wrap(policy::GetChromeSchemaData());
  policy::PolicyMap policy_map;
  policy::PolicyErrorMap errors;
  PrefValueMap prefs;
  ExtensionSettingsPolicyHandler handler(chrome_schema);

  policy_map.Set(policy::key::kExtensionSettings,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, std::move(*policy_result),
                 nullptr);
  // CheckPolicySettings() has an error message because of the missing update
  // URL.
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());

  // ApplyPolicySettings() gives an empty policy.
  handler.ApplyPolicySettings(policy_map, &prefs);
  base::Value* value = nullptr;
  ASSERT_TRUE(prefs.GetValue(pref_names::kExtensionManagement, &value));
  EXPECT_EQ(*stripped_policy_result, *value);
}

// Only enterprise managed machines can auto install extensions from a location
// other than the webstore https://crbug.com/809004.
TEST(ExtensionSettingsPolicyHandlerTest, NonManagedOffWebstoreExtension) {
  // Mark as not enterprise managed.
  auto policy_result = base::JSONReader::ReadAndReturnValueWithError(
      kSensitiveTestManagementPolicy, kJsonParseOptions);
  ASSERT_TRUE(policy_result.has_value()) << policy_result.error().message;

  auto sanitized_policy_result = base::JSONReader::ReadAndReturnValueWithError(
      kSanitizedTestManagementPolicy, kJsonParseOptions);
  ASSERT_TRUE(sanitized_policy_result.has_value())
      << sanitized_policy_result.error().message;

  policy::Schema chrome_schema =
      policy::Schema::Wrap(policy::GetChromeSchemaData());
  policy::PolicyMap policy_map;
  policy::PolicyErrorMap errors;
  PrefValueMap prefs;
  ExtensionSettingsPolicyHandler handler(chrome_schema);

  policy_map.Set(policy::key::kExtensionSettings,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, policy_result->Clone(), nullptr);
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());

  handler.ApplyPolicySettings(policy_map, &prefs);
  base::Value* value = nullptr;
  ASSERT_TRUE(prefs.GetValue(pref_names::kExtensionManagement, &value));
  EXPECT_EQ(*sanitized_policy_result, *value);
}

}  // namespace extensions
