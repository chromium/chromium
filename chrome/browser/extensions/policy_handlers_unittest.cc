// Copyright 2013 The Chromium Authors. All rights reserved.
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

#if defined(OS_WIN)
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

TEST(ExtensionListPolicyHandlerTest, CheckPolicySettings) {
  base::ListValue list;
  policy::PolicyMap policy_map;
  policy::PolicyErrorMap errors;
  ExtensionListPolicyHandler handler(
      policy::key::kExtensionInstallBlacklist, kTestPref, true);

  policy_map.Set(policy::key::kExtensionInstallBlacklist,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, list.CreateDeepCopy(), nullptr);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  list.AppendString("abcdefghijklmnopabcdefghijklmnop");
  policy_map.Set(policy::key::kExtensionInstallBlacklist,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, list.CreateDeepCopy(), nullptr);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  list.AppendString("*");
  policy_map.Set(policy::key::kExtensionInstallBlacklist,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, list.CreateDeepCopy(), nullptr);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  list.AppendString("invalid");
  policy_map.Set(policy::key::kExtensionInstallBlacklist,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, list.CreateDeepCopy(), nullptr);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());
  EXPECT_FALSE(
      errors.GetErrors(policy::key::kExtensionInstallBlacklist).empty());
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
    std::string error;
    std::unique_ptr<base::Value> policy_value =
        base::JSONReader::ReadAndReturnErrorDeprecated(
            policy, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS,
            nullptr, &error);
    if (!policy_value)
      return false;

    policy::Schema chrome_schema =
        policy::Schema::Wrap(policy::GetChromeSchemaData());
    policy::PolicyMap policy_map;
    ExtensionSettingsPolicyHandler handler(chrome_schema);

    policy_map.Set(policy::key::kExtensionSettings,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD, std::move(policy_value),
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
  base::ListValue policy;
  base::ListValue expected;
  policy::PolicyMap policy_map;
  PrefValueMap prefs;
  base::Value* value = NULL;
  ExtensionListPolicyHandler handler(
      policy::key::kExtensionInstallBlacklist, kTestPref, false);

  policy.AppendString("abcdefghijklmnopabcdefghijklmnop");
  expected.AppendString("abcdefghijklmnopabcdefghijklmnop");

  policy_map.Set(policy::key::kExtensionInstallBlacklist,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, policy.CreateDeepCopy(), nullptr);
  handler.ApplyPolicySettings(policy_map, &prefs);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_EQ(expected, *value);

  policy.AppendString("invalid");
  policy_map.Set(policy::key::kExtensionInstallBlacklist,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, policy.CreateDeepCopy(), nullptr);
  handler.ApplyPolicySettings(policy_map, &prefs);
  EXPECT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_EQ(expected, *value);
}

TEST(ExtensionInstallForcelistPolicyHandlerTest, CheckPolicySettings) {
  base::ListValue list;
  policy::PolicyMap policy_map;
  policy::PolicyErrorMap errors;
  ExtensionInstallForcelistPolicyHandler handler;

  // Start with an empty policy.
  policy_map.Set(policy::key::kExtensionInstallForcelist,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, list.CreateDeepCopy(), nullptr);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  // Add a correct entry. No errors should be generated.
  list.AppendString("abcdefghijklmnopabcdefghijklmnop;http://example.com");
  policy_map.Set(policy::key::kExtensionInstallForcelist,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, list.CreateDeepCopy(), nullptr);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  // Add an erroneous entry. This should generate an error, but the good
  // entry should still be translated successfully.
  list.AppendString("adfasdf;http://example.com");
  policy_map.Set(policy::key::kExtensionInstallForcelist,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, list.CreateDeepCopy(), nullptr);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_EQ(1U, errors.size());

  // Add an entry with bad URL, which should generate another error.
  list.AppendString("abcdefghijklmnopabcdefghijklmnop;nourl");
  policy_map.Set(policy::key::kExtensionInstallForcelist,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, list.CreateDeepCopy(), nullptr);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_EQ(2U, errors.size());

  // Just an extension ID should be accepted.
  list.AppendString("abcdefghijklmnopabcdefghijklmnop");
  policy_map.Set(policy::key::kExtensionInstallForcelist,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, list.CreateDeepCopy(), nullptr);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_EQ(2U, errors.size());
}

TEST(ExtensionInstallForcelistPolicyHandlerTest, ApplyPolicySettings) {
  base::ListValue policy;
  base::DictionaryValue expected;
  policy::PolicyMap policy_map;
  PrefValueMap prefs;
  base::Value* value = NULL;
  ExtensionInstallForcelistPolicyHandler handler;

  // Start with the policy being missing. This shouldn't affect the pref.
  handler.ApplyPolicySettings(policy_map, &prefs);
  EXPECT_FALSE(prefs.GetValue(pref_names::kInstallForceList, &value));
  EXPECT_FALSE(value);

  // Set the policy to an empty value. This shouldn't affect the pref.
  policy_map.Set(policy::key::kExtensionInstallForcelist,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, policy.CreateDeepCopy(), nullptr);
  handler.ApplyPolicySettings(policy_map, &prefs);
  EXPECT_TRUE(prefs.GetValue(pref_names::kInstallForceList, &value));
  EXPECT_EQ(expected, *value);

  // Add a correct entry to the policy. The pref should contain a corresponding
  // entry.
  policy.AppendString("abcdefghijklmnopabcdefghijklmnop;http://example.com");
  extensions::ExternalPolicyLoader::AddExtension(
      &expected, "abcdefghijklmnopabcdefghijklmnop", "http://example.com");
  policy_map.Set(policy::key::kExtensionInstallForcelist,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, policy.CreateDeepCopy(), nullptr);
  handler.ApplyPolicySettings(policy_map, &prefs);
  EXPECT_TRUE(prefs.GetValue(pref_names::kInstallForceList, &value));
  EXPECT_EQ(expected, *value);

  // Add a correct entry with an omitted update URL. The pref should contain now
  // two entries, with the default update URL substituted for the new entry.
  // Note: the URL hardcoded below is part of the public policy contract (as
  // documented in the policy_templates.json file), and therefore any changes to
  // it must be carefully thought out.
  policy.AppendString("bcdefghijklmnopabcdefghijklmnopa");
  extensions::ExternalPolicyLoader::AddExtension(
      &expected, "bcdefghijklmnopabcdefghijklmnopa",
      "https://clients2.google.com/service/update2/crx");
  policy_map.Set(policy::key::kExtensionInstallForcelist,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, policy.CreateDeepCopy(), nullptr);
  handler.ApplyPolicySettings(policy_map, &prefs);
  EXPECT_TRUE(prefs.GetValue(pref_names::kInstallForceList, &value));
  EXPECT_EQ(expected, *value);

  // Add an invalid entry. The pref should still contain two previous entries.
  policy.AppendString("invalid");
  policy_map.Set(policy::key::kExtensionInstallForcelist,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, policy.CreateDeepCopy(), nullptr);
  handler.ApplyPolicySettings(policy_map, &prefs);
  EXPECT_TRUE(prefs.GetValue(pref_names::kInstallForceList, &value));
  EXPECT_EQ(expected, *value);
}

TEST(ExtensionURLPatternListPolicyHandlerTest, CheckPolicySettings) {
  base::ListValue list;
  policy::PolicyMap policy_map;
  policy::PolicyErrorMap errors;
  ExtensionURLPatternListPolicyHandler handler(
      policy::key::kExtensionInstallSources, kTestPref);

  policy_map.Set(policy::key::kExtensionInstallSources,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, list.CreateDeepCopy(), nullptr);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  list.AppendString("http://*.google.com/*");
  policy_map.Set(policy::key::kExtensionInstallSources,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, list.CreateDeepCopy(), nullptr);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  list.AppendString("<all_urls>");
  policy_map.Set(policy::key::kExtensionInstallSources,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, list.CreateDeepCopy(), nullptr);
  errors.Clear();
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());

  list.AppendString("invalid");
  policy_map.Set(policy::key::kExtensionInstallSources,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, list.CreateDeepCopy(), nullptr);
  errors.Clear();
  EXPECT_FALSE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());
  EXPECT_FALSE(errors.GetErrors(policy::key::kExtensionInstallSources).empty());

  // URLPattern syntax has a different way to express 'all urls'. Though '*'
  // would be compatible today, it would be brittle, so we disallow.
  list.AppendString("*");
  policy_map.Set(policy::key::kExtensionInstallSources,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, list.CreateDeepCopy(), nullptr);
  errors.Clear();
  EXPECT_FALSE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());
  EXPECT_FALSE(errors.GetErrors(policy::key::kExtensionInstallSources).empty());
}

TEST(ExtensionURLPatternListPolicyHandlerTest, ApplyPolicySettings) {
  base::ListValue list;
  policy::PolicyMap policy_map;
  PrefValueMap prefs;
  base::Value* value = NULL;
  ExtensionURLPatternListPolicyHandler handler(
      policy::key::kExtensionInstallSources, kTestPref);

  list.AppendString("https://corp.monkey.net/*");
  policy_map.Set(policy::key::kExtensionInstallSources,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, list.CreateDeepCopy(), nullptr);
  handler.ApplyPolicySettings(policy_map, &prefs);
  ASSERT_TRUE(prefs.GetValue(kTestPref, &value));
  EXPECT_EQ(list, *value);
}

TEST(ExtensionSettingsPolicyHandlerTest, CheckPolicySettings) {
  std::string error;
  std::unique_ptr<base::Value> policy_value =
      base::JSONReader::ReadAndReturnErrorDeprecated(
          kTestManagementPolicy1,
          base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS, NULL, &error);
  ASSERT_TRUE(policy_value.get()) << error;

  policy::Schema chrome_schema =
      policy::Schema::Wrap(policy::GetChromeSchemaData());
  policy::PolicyMap policy_map;
  policy::PolicyErrorMap errors;
  ExtensionSettingsPolicyHandler handler(chrome_schema);

  policy_map.Set(policy::key::kExtensionSettings,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, std::move(policy_value), nullptr);
  // CheckPolicySettings() fails due to missing update URL.
  EXPECT_FALSE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());
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
  auto policy_value = base::JSONReader::ReadAndReturnValueWithError(
      policy, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);
  policy::Schema chrome_schema =
      policy::Schema::Wrap(policy::GetChromeSchemaData());
  policy::PolicyMap policy_map;
  policy::PolicyErrorMap errors;
  ExtensionSettingsPolicyHandler handler(chrome_schema);

  policy_map.Set(policy::key::kExtensionSettings,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD,
                 policy_value.value.value().CreateDeepCopy(), nullptr);

  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_EQ(2u, errors.size());
  auto error_str = errors.GetErrors(policy::key::kExtensionSettings);
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
#if defined(OS_WIN)
  base::win::ScopedDomainStateForTesting scoped_domain(true);
#endif

  std::string error;
  std::unique_ptr<base::Value> policy_value =
      base::JSONReader::ReadAndReturnErrorDeprecated(
          kTestManagementPolicy2,
          base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS, NULL, &error);
  ASSERT_TRUE(policy_value.get()) << error;

  policy::Schema chrome_schema =
      policy::Schema::Wrap(policy::GetChromeSchemaData());
  policy::PolicyMap policy_map;
  policy::PolicyErrorMap errors;
  PrefValueMap prefs;
  ExtensionSettingsPolicyHandler handler(chrome_schema);

  policy_map.Set(policy::key::kExtensionSettings,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, policy_value->CreateDeepCopy(),
                 nullptr);
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  handler.ApplyPolicySettings(policy_map, &prefs);
  base::Value* value = NULL;
  ASSERT_TRUE(prefs.GetValue(pref_names::kExtensionManagement, &value));
  EXPECT_EQ(*policy_value, *value);
}

// Only enterprise managed machines can auto install extensions from a location
// other than the webstore https://crbug.com/809004.
#if defined(OS_WIN)
TEST(ExtensionSettingsPolicyHandlerTest, NonManagedOffWebstoreExtension) {
  // Mark as not enterprise managed.
  base::win::ScopedDomainStateForTesting scoped_domain(false);

  std::string error;
  std::unique_ptr<base::Value> policy_value =
      base::JSONReader::ReadAndReturnErrorDeprecated(
          kTestManagementPolicy2,
          base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS, nullptr, &error);
  ASSERT_TRUE(policy_value.get()) << error;

  policy::Schema chrome_schema =
      policy::Schema::Wrap(policy::GetChromeSchemaData());
  policy::PolicyMap policy_map;
  policy::PolicyErrorMap errors;
  PrefValueMap prefs;
  ExtensionSettingsPolicyHandler handler(chrome_schema);

  policy_map.Set(policy::key::kExtensionSettings,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, policy_value->CreateDeepCopy(),
                 nullptr);
  EXPECT_FALSE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.empty());
}
#endif

}  // namespace extensions
