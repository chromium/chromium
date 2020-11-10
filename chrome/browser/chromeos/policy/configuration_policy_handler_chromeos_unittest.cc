// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/configuration_policy_handler_chromeos.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/ash_pref_names.h"
#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/ui/ash/chrome_launcher_prefs.h"
#include "chrome/common/pref_names.h"
#include "components/arc/arc_prefs.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

// Test cases for the screen magnifier type policy setting.
class ScreenMagnifierPolicyHandlerTest : public testing::Test {
 protected:
  PolicyMap policy_;
  PrefValueMap prefs_;
  ScreenMagnifierPolicyHandler handler_;
};

class LoginScreenPowerManagementPolicyHandlerTest : public testing::Test {
 protected:
  LoginScreenPowerManagementPolicyHandlerTest();

  void SetUp() override;

  Schema chrome_schema_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginScreenPowerManagementPolicyHandlerTest);
};

LoginScreenPowerManagementPolicyHandlerTest::
    LoginScreenPowerManagementPolicyHandlerTest() {
}

void LoginScreenPowerManagementPolicyHandlerTest::SetUp() {
  chrome_schema_ = Schema::Wrap(GetChromeSchemaData());
}

TEST_F(ScreenMagnifierPolicyHandlerTest, Default) {
  handler_.ApplyPolicySettings(policy_, &prefs_);
  EXPECT_FALSE(
      prefs_.GetValue(ash::prefs::kAccessibilityScreenMagnifierEnabled, NULL));
}

TEST_F(ScreenMagnifierPolicyHandlerTest, Disabled) {
  policy_.Set(key::kScreenMagnifierType, POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(0), nullptr);
  handler_.ApplyPolicySettings(policy_, &prefs_);

  const base::Value* enabled = nullptr;
  EXPECT_TRUE(prefs_.GetValue(ash::prefs::kAccessibilityScreenMagnifierEnabled,
                              &enabled));
  ASSERT_TRUE(enabled);
  EXPECT_EQ(base::Value(false), *enabled);
}

TEST_F(ScreenMagnifierPolicyHandlerTest, Enabled) {
  policy_.Set(key::kScreenMagnifierType, POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(1), nullptr);
  handler_.ApplyPolicySettings(policy_, &prefs_);

  const base::Value* enabled = nullptr;
  EXPECT_TRUE(prefs_.GetValue(ash::prefs::kAccessibilityScreenMagnifierEnabled,
                              &enabled));
  ASSERT_TRUE(enabled);
  EXPECT_EQ(base::Value(true), *enabled);
}

TEST(ExternalDataPolicyHandlerTest, Empty) {
  PolicyErrorMap errors;
  EXPECT_TRUE(ExternalDataPolicyHandler(key::kUserAvatarImage)
                  .CheckPolicySettings(PolicyMap(), &errors));
  EXPECT_TRUE(errors.GetErrors(key::kUserAvatarImage).empty());
}

TEST(ExternalDataPolicyHandlerTest, WrongType) {
  PolicyMap policy_map;
  policy_map.Set(key::kUserAvatarImage, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(false),
                 nullptr);
  PolicyErrorMap errors;
  EXPECT_FALSE(ExternalDataPolicyHandler(key::kUserAvatarImage)
                   .CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.GetErrors(key::kUserAvatarImage).empty());
}

TEST(ExternalDataPolicyHandlerTest, MissingURL) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("hash", "1234567890123456789012345678901234567890");
  PolicyMap policy_map;
  policy_map.Set(key::kUserAvatarImage, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, std::move(dict),
                 nullptr);
  PolicyErrorMap errors;
  EXPECT_FALSE(ExternalDataPolicyHandler(key::kUserAvatarImage)
                   .CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.GetErrors(key::kUserAvatarImage).empty());
}

TEST(ExternalDataPolicyHandlerTest, InvalidURL) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("url", "http://");
  dict.SetStringKey("hash", "1234567890123456789012345678901234567890");
  PolicyMap policy_map;
  policy_map.Set(key::kUserAvatarImage, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, std::move(dict),
                 nullptr);
  PolicyErrorMap errors;
  EXPECT_FALSE(ExternalDataPolicyHandler(key::kUserAvatarImage)
                   .CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.GetErrors(key::kUserAvatarImage).empty());
}

TEST(ExternalDataPolicyHandlerTest, MissingHash) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("url", "http://localhost/");
  PolicyMap policy_map;
  policy_map.Set(key::kUserAvatarImage, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, std::move(dict),
                 nullptr);
  PolicyErrorMap errors;
  EXPECT_FALSE(ExternalDataPolicyHandler(key::kUserAvatarImage)
                   .CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.GetErrors(key::kUserAvatarImage).empty());
}

TEST(ExternalDataPolicyHandlerTest, InvalidHash) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("url", "http://localhost/");
  dict.SetStringKey("hash", "1234");
  PolicyMap policy_map;
  policy_map.Set(key::kUserAvatarImage, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, std::move(dict),
                 nullptr);
  PolicyErrorMap errors;
  EXPECT_FALSE(ExternalDataPolicyHandler(key::kUserAvatarImage)
                   .CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.GetErrors(key::kUserAvatarImage).empty());
}

TEST(ExternalDataPolicyHandlerTest, Valid) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("url", "http://localhost/");
  dict.SetStringKey(
      "hash",
      "1234567890123456789012345678901234567890123456789012345678901234");
  PolicyMap policy_map;
  policy_map.Set(key::kUserAvatarImage, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, std::move(dict),
                 nullptr);
  PolicyErrorMap errors;
  EXPECT_TRUE(ExternalDataPolicyHandler(key::kUserAvatarImage)
                  .CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.GetErrors(key::kUserAvatarImage).empty());
}

const char kLoginScreenPowerManagementPolicy[] =
    "{"
    "  \"AC\": {"
    "    \"Delays\": {"
    "      \"ScreenDim\": 5000,"
    "      \"ScreenOff\": 7000,"
    "      \"Idle\": 9000"
    "    },"
    "    \"IdleAction\": \"DoNothing\""
    "  },"
    "  \"Battery\": {"
    "    \"Delays\": {"
    "      \"ScreenDim\": 1000,"
    "      \"ScreenOff\": 3000,"
    "      \"Idle\": 4000"
    "    },"
    "    \"IdleAction\": \"DoNothing\""
    "  },"
    "  \"LidCloseAction\": \"DoNothing\","
    "  \"UserActivityScreenDimDelayScale\": 300"
    "}";

}  // namespace

TEST(NetworkConfigurationPolicyHandlerTest, Empty) {
  PolicyMap policy_map;
  std::unique_ptr<NetworkConfigurationPolicyHandler> handler(
      NetworkConfigurationPolicyHandler::CreateForUserPolicy());
  PolicyErrorMap errors;
  EXPECT_TRUE(handler->CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.GetErrors(key::kOpenNetworkConfiguration).empty());
}

TEST(NetworkConfigurationPolicyHandlerTest, ValidONC) {
  const std::string kTestONC(
      "{"
      "  \"NetworkConfigurations\": [{"
      "    \"GUID\": \"{485d6076-dd44-6b6d-69787465725f5045}\","
      "    \"Type\": \"WiFi\","
      "    \"Name\": \"some name\","
      "    \"WiFi\": {"
      "      \"Security\": \"WEP-PSK\","
      "      \"SSID\": \"ssid\","
      "      \"Passphrase\": \"pass\","
      "    }"
      "  }]"
      "}");

  PolicyMap policy_map;
  policy_map.Set(key::kOpenNetworkConfiguration, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(kTestONC),
                 nullptr);
  std::unique_ptr<NetworkConfigurationPolicyHandler> handler(
      NetworkConfigurationPolicyHandler::CreateForUserPolicy());
  PolicyErrorMap errors;
  EXPECT_TRUE(handler->CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.GetErrors(key::kOpenNetworkConfiguration).empty());
}

TEST(NetworkConfigurationPolicyHandlerTest, WrongType) {
  PolicyMap policy_map;
  policy_map.Set(key::kOpenNetworkConfiguration, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(false),
                 nullptr);
  std::unique_ptr<NetworkConfigurationPolicyHandler> handler(
      NetworkConfigurationPolicyHandler::CreateForUserPolicy());
  PolicyErrorMap errors;
  EXPECT_FALSE(handler->CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.GetErrors(key::kOpenNetworkConfiguration).empty());
}

TEST(NetworkConfigurationPolicyHandlerTest, JSONParseError) {
  const std::string kTestONC("I'm not proper JSON!");
  PolicyMap policy_map;
  policy_map.Set(key::kOpenNetworkConfiguration, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(kTestONC),
                 nullptr);
  std::unique_ptr<NetworkConfigurationPolicyHandler> handler(
      NetworkConfigurationPolicyHandler::CreateForUserPolicy());
  PolicyErrorMap errors;
  EXPECT_FALSE(handler->CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.GetErrors(key::kOpenNetworkConfiguration).empty());
}

TEST(NetworkConfigurationPolicyHandlerTest, Sanitization) {
  const std::string kTestONC(
      "{"
      "  \"NetworkConfigurations\": [{"
      "    \"GUID\": \"{485d6076-dd44-6b6d-69787465725f5045}\","
      "    \"Type\": \"WiFi\","
      "    \"Name\": \"some name\","
      "    \"WiFi\": {"
      "      \"Security\": \"WEP-PSK\","
      "      \"SSID\": \"ssid\","
      "      \"Passphrase\": \"pass\","
      "    }"
      "  }]"
      "}");

  PolicyMap policy_map;
  policy_map.Set(key::kOpenNetworkConfiguration, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(kTestONC),
                 nullptr);
  std::unique_ptr<NetworkConfigurationPolicyHandler> handler(
      NetworkConfigurationPolicyHandler::CreateForUserPolicy());
  PolicyErrorMap errors;
  handler->PrepareForDisplaying(&policy_map);
  const base::Value* sanitized =
      policy_map.GetValue(key::kOpenNetworkConfiguration);
  ASSERT_TRUE(sanitized);
  std::string sanitized_onc;
  EXPECT_TRUE(sanitized->GetAsString(&sanitized_onc));
  EXPECT_FALSE(sanitized_onc.empty());
  EXPECT_EQ(std::string::npos, sanitized_onc.find("pass"));
}

TEST(PinnedLauncherAppsPolicyHandler, PrefTranslation) {
  base::Value list(base::Value::Type::LIST);
  PolicyMap policy_map;
  PrefValueMap prefs;
  base::ListValue expected_pinned_apps;
  base::Value* value = nullptr;
  PinnedLauncherAppsPolicyHandler handler;

  policy_map.Set(key::kPinnedLauncherApps, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, list.Clone(), nullptr);
  handler.ApplyPolicySettings(policy_map, &prefs);
  EXPECT_TRUE(prefs.GetValue(prefs::kPolicyPinnedLauncherApps, &value));
  EXPECT_EQ(expected_pinned_apps, *value);

  // Extension IDs are OK.
  base::Value entry1("abcdefghijklmnopabcdefghijklmnop");
  base::Value entry1_dict(base::Value::Type::DICTIONARY);
  entry1_dict.SetKey(kPinnedAppsPrefAppIDKey, entry1.Clone());
  expected_pinned_apps.Append(std::move(entry1_dict));
  list.Append(entry1.Clone());

  // Android appds are OK.
  base::Value entry2("com.google.android.gm");
  auto entry2_dict = base::Value(base::Value::Type::DICTIONARY);
  entry2_dict.SetKey(kPinnedAppsPrefAppIDKey, entry2.Clone());
  expected_pinned_apps.Append(std::move(entry2_dict));
  list.Append(entry2.Clone());

  // Anything else is not OK.
  base::Value entry3("invalid");
  list.Append(entry3.Clone());

  policy_map.Set(key::kPinnedLauncherApps, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, list.Clone(), nullptr);
  prefs.Clear();
  handler.ApplyPolicySettings(policy_map, &prefs);
  EXPECT_TRUE(prefs.GetValue(prefs::kPolicyPinnedLauncherApps, &value));
  EXPECT_EQ(expected_pinned_apps, *value);
}

TEST_F(LoginScreenPowerManagementPolicyHandlerTest, Empty) {
  PolicyMap policy_map;
  LoginScreenPowerManagementPolicyHandler handler(chrome_schema_);
  PolicyErrorMap errors;
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.GetErrors(key::kDeviceLoginScreenPowerManagement).empty());
}

TEST_F(LoginScreenPowerManagementPolicyHandlerTest, ValidPolicy) {
  PolicyMap policy_map;
  policy_map.Set(key::kDeviceLoginScreenPowerManagement, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                 base::JSONReader::Read(kLoginScreenPowerManagementPolicy),
                 nullptr);
  LoginScreenPowerManagementPolicyHandler handler(chrome_schema_);
  PolicyErrorMap errors;
  EXPECT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.GetErrors(key::kDeviceLoginScreenPowerManagement).empty());
}

TEST_F(LoginScreenPowerManagementPolicyHandlerTest, WrongType) {
  PolicyMap policy_map;
  policy_map.Set(key::kDeviceLoginScreenPowerManagement, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(false),
                 nullptr);
  LoginScreenPowerManagementPolicyHandler handler(chrome_schema_);
  PolicyErrorMap errors;
  EXPECT_FALSE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(
      errors.GetErrors(key::kDeviceLoginScreenPowerManagement).empty());
}

TEST(ArcServicePolicyHandlerTest, DisabledByDefault) {
  PolicyMap policy_map;
  SetEnterpriseUsersDefaults(&policy_map);
  ArcServicePolicyHandler handler(key::kArcBackupRestoreServiceEnabled,
                                  arc::prefs::kArcBackupRestoreEnabled);
  PolicyErrorMap errors;
  ASSERT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());
  PrefValueMap prefs;
  handler.ApplyPolicySettings(policy_map, &prefs);
  const base::Value* enabled = nullptr;
  EXPECT_TRUE(prefs.GetValue(arc::prefs::kArcBackupRestoreEnabled, &enabled));
  ASSERT_TRUE(enabled);
  EXPECT_EQ(base::Value(false), *enabled);
}

TEST(ArcServicePolicyHandlerTest, UnderUserControlWhenWrongType) {
  PolicyMap policy_map;
  policy_map.Set(key::kArcBackupRestoreServiceEnabled, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(false),
                 nullptr);
  SetEnterpriseUsersDefaults(&policy_map);
  ArcServicePolicyHandler handler(key::kArcBackupRestoreServiceEnabled,
                                  arc::prefs::kArcBackupRestoreEnabled);
  PolicyErrorMap errors;
  EXPECT_FALSE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.GetErrors(key::kArcBackupRestoreServiceEnabled).empty());
}

TEST(ArcServicePolicyHandlerTest, UnderUserControlWhenOutOfRange) {
  PolicyMap policy_map;
  policy_map.Set(key::kArcBackupRestoreServiceEnabled, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(3),
                 nullptr);
  SetEnterpriseUsersDefaults(&policy_map);
  ArcServicePolicyHandler handler(key::kArcBackupRestoreServiceEnabled,
                                  arc::prefs::kArcBackupRestoreEnabled);
  PolicyErrorMap errors;
  EXPECT_FALSE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.GetErrors(key::kArcBackupRestoreServiceEnabled).empty());
}

TEST(ArcServicePolicyHandlerTest, DisabledByPolicy) {
  PolicyMap policy_map;
  policy_map.Set(
      key::kArcBackupRestoreServiceEnabled, POLICY_LEVEL_MANDATORY,
      POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
      base::Value(static_cast<int>(ArcServicePolicyValue::kDisabled)), nullptr);
  SetEnterpriseUsersDefaults(&policy_map);
  ArcServicePolicyHandler handler(key::kArcBackupRestoreServiceEnabled,
                                  arc::prefs::kArcBackupRestoreEnabled);
  PolicyErrorMap errors;
  ASSERT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());
  PrefValueMap prefs;
  handler.ApplyPolicySettings(policy_map, &prefs);
  const base::Value* enabled = nullptr;
  EXPECT_TRUE(prefs.GetValue(arc::prefs::kArcBackupRestoreEnabled, &enabled));
  ASSERT_TRUE(enabled);
  EXPECT_EQ(base::Value(false), *enabled);
}

TEST(ArcServicePolicyHandlerTest, UnderUserControlByPolicy) {
  PolicyMap policy_map;
  policy_map.Set(
      key::kArcBackupRestoreServiceEnabled, POLICY_LEVEL_MANDATORY,
      POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
      base::Value(static_cast<int>(ArcServicePolicyValue::kUnderUserControl)),
      nullptr);
  SetEnterpriseUsersDefaults(&policy_map);
  ArcServicePolicyHandler handler(key::kArcBackupRestoreServiceEnabled,
                                  arc::prefs::kArcBackupRestoreEnabled);
  PolicyErrorMap errors;
  ASSERT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());
  PrefValueMap prefs;
  handler.ApplyPolicySettings(policy_map, &prefs);
  const base::Value* enabled = nullptr;
  EXPECT_FALSE(prefs.GetValue(arc::prefs::kArcBackupRestoreEnabled, &enabled));
}

TEST(ArcServicePolicyHandlerTest, EnabledByPolicy) {
  PolicyMap policy_map;
  policy_map.Set(key::kArcBackupRestoreServiceEnabled, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                 base::Value(static_cast<int>(ArcServicePolicyValue::kEnabled)),
                 nullptr);
  SetEnterpriseUsersDefaults(&policy_map);
  ArcServicePolicyHandler handler(key::kArcBackupRestoreServiceEnabled,
                                  arc::prefs::kArcBackupRestoreEnabled);
  PolicyErrorMap errors;
  ASSERT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());
  PrefValueMap prefs;
  handler.ApplyPolicySettings(policy_map, &prefs);
  const base::Value* enabled = nullptr;
  EXPECT_TRUE(prefs.GetValue(arc::prefs::kArcBackupRestoreEnabled, &enabled));
  ASSERT_TRUE(enabled);
  EXPECT_EQ(base::Value(true), *enabled);
}

TEST(ArcServicePolicyHandlerTest, NotOverridingAnotherPolicy) {
  PolicyMap policy_map;
  policy_map.Set(key::kArcBackupRestoreServiceEnabled, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                 base::Value(static_cast<int>(ArcServicePolicyValue::kEnabled)),
                 nullptr);
  SetEnterpriseUsersDefaults(&policy_map);
  ArcServicePolicyHandler handler(key::kArcBackupRestoreServiceEnabled,
                                  arc::prefs::kArcBackupRestoreEnabled);
  PolicyErrorMap errors;
  ASSERT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());
  PrefValueMap prefs;
  prefs.SetBoolean(arc::prefs::kArcBackupRestoreEnabled, false);
  handler.ApplyPolicySettings(policy_map, &prefs);
  const base::Value* enabled = nullptr;
  EXPECT_TRUE(prefs.GetValue(arc::prefs::kArcBackupRestoreEnabled, &enabled));
  ASSERT_TRUE(enabled);
  EXPECT_EQ(base::Value(false), *enabled);
}

TEST(ArcServicePolicyHandlerTest, UnserUserControlForConsumer) {
  PolicyMap policy_map;
  ArcServicePolicyHandler handler(key::kArcBackupRestoreServiceEnabled,
                                  arc::prefs::kArcBackupRestoreEnabled);
  PolicyErrorMap errors;
  ASSERT_TRUE(handler.CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.empty());
  PrefValueMap prefs;
  handler.ApplyPolicySettings(policy_map, &prefs);
  const base::Value* enabled = nullptr;
  EXPECT_FALSE(prefs.GetValue(arc::prefs::kArcBackupRestoreEnabled, &enabled));
}

}  // namespace policy
