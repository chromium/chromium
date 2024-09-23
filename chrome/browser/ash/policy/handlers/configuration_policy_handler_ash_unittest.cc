// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/configuration_policy_handler_ash.h"

#include <memory>
#include <utility>

#include "ash/components/arc/arc_prefs.h"
#include "ash/constants/ash_pref_names.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_prefs.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/power/power_policy_controller.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/cloud/mock_cloud_external_data_manager.h"
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

class PowerManagementIdleSettingsPolicyHandlerTest : public testing::Test {
 protected:
  PolicyMap policy_;
  PrefValueMap prefs_;
  Schema chrome_schema_{Schema::Wrap(GetChromeSchemaData())};
};

class LoginScreenPowerManagementPolicyHandlerTest : public testing::Test {
 protected:
  LoginScreenPowerManagementPolicyHandlerTest();

  LoginScreenPowerManagementPolicyHandlerTest(
      const LoginScreenPowerManagementPolicyHandlerTest&) = delete;
  LoginScreenPowerManagementPolicyHandlerTest& operator=(
      const LoginScreenPowerManagementPolicyHandlerTest&) = delete;

  void SetUp() override;

  Schema chrome_schema_;
};

LoginScreenPowerManagementPolicyHandlerTest::
    LoginScreenPowerManagementPolicyHandlerTest() {}

void LoginScreenPowerManagementPolicyHandlerTest::SetUp() {
  chrome_schema_ = Schema::Wrap(GetChromeSchemaData());
}

// Test cases for the Help me write policy setting.
class HelpMeWritePolicyHandlerTest : public testing::Test {
 protected:
  PolicyMap policy_;
  PrefValueMap prefs_;
  HelpMeWritePolicyHandler handler_;
};

base::Value GetPref(PrefValueMap* prefs, const std::string& name) {
  base::Value* pref_value = nullptr;
  if (prefs->GetValue(name, &pref_value)) {
    return pref_value->Clone();
  }
  return base::Value("Pref was not found");
}

TEST_F(ScreenMagnifierPolicyHandlerTest, Default) {
  handler_.ApplyPolicySettings(policy_, &prefs_);
  EXPECT_FALSE(prefs_.GetValue(ash::prefs::kAccessibilityScreenMagnifierEnabled,
                               nullptr));
}

TEST_F(ScreenMagnifierPolicyHandlerTest, Disabled) {
  policy_.Set(key::kScreenMagnifierType, POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(0), nullptr);
  handler_.ApplyPolicySettings(policy_, &prefs_);
  EXPECT_EQ(base::Value(false),
            GetPref(&prefs_, ash::prefs::kAccessibilityScreenMagnifierEnabled));
}

TEST_F(ScreenMagnifierPolicyHandlerTest, Enabled) {
  policy_.Set(key::kScreenMagnifierType, POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(1), nullptr);
  handler_.ApplyPolicySettings(policy_, &prefs_);
  EXPECT_EQ(base::Value(true),
            GetPref(&prefs_, ash::prefs::kAccessibilityScreenMagnifierEnabled));
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
  auto dict = base::Value::Dict().Set(
      "hash", "1234567890123456789012345678901234567890");
  PolicyMap policy_map;
  policy_map.Set(key::kUserAvatarImage, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                 base::Value(std::move(dict)), nullptr);
  PolicyErrorMap errors;
  EXPECT_FALSE(ExternalDataPolicyHandler(key::kUserAvatarImage)
                   .CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.GetErrors(key::kUserAvatarImage).empty());
}

TEST(ExternalDataPolicyHandlerTest, InvalidURL) {
  auto dict = base::Value::Dict()
                  .Set("url", "http://")
                  .Set("hash", "1234567890123456789012345678901234567890");
  PolicyMap policy_map;
  policy_map.Set(key::kUserAvatarImage, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                 base::Value(std::move(dict)), nullptr);
  PolicyErrorMap errors;
  EXPECT_FALSE(ExternalDataPolicyHandler(key::kUserAvatarImage)
                   .CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.GetErrors(key::kUserAvatarImage).empty());
}

TEST(ExternalDataPolicyHandlerTest, MissingHash) {
  auto dict = base::Value::Dict().Set("url", "http://localhost/");
  PolicyMap policy_map;
  policy_map.Set(key::kUserAvatarImage, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                 base::Value(std::move(dict)), nullptr);
  PolicyErrorMap errors;
  EXPECT_FALSE(ExternalDataPolicyHandler(key::kUserAvatarImage)
                   .CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.GetErrors(key::kUserAvatarImage).empty());
}

TEST(ExternalDataPolicyHandlerTest, InvalidHash) {
  auto dict =
      base::Value::Dict().Set("url", "http://localhost/").Set("hash", "1234");
  PolicyMap policy_map;
  policy_map.Set(key::kUserAvatarImage, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                 base::Value(std::move(dict)), nullptr);
  PolicyErrorMap errors;
  EXPECT_FALSE(ExternalDataPolicyHandler(key::kUserAvatarImage)
                   .CheckPolicySettings(policy_map, &errors));
  EXPECT_FALSE(errors.GetErrors(key::kUserAvatarImage).empty());
}

TEST(ExternalDataPolicyHandlerTest, Valid) {
  auto dict = base::Value::Dict()
                  .Set("url", "http://localhost/")
                  .Set("hash",
                       "1234567890123456789012345678901234567890123456789012345"
                       "678901234");
  PolicyMap policy_map;
  MockCloudExternalDataManager external_data_manager;

  policy_map.Set(
      key::kUserAvatarImage, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
      POLICY_SOURCE_CLOUD, base::Value(std::move(dict)),
      external_data_manager.CreateExternalDataFetcher(key::kUserAvatarImage));
  PolicyErrorMap errors;
  EXPECT_TRUE(ExternalDataPolicyHandler(key::kUserAvatarImage)
                  .CheckPolicySettings(policy_map, &errors));
  EXPECT_TRUE(errors.GetErrors(key::kUserAvatarImage).empty());
}

const char kLoginScreenPowerManagementPolicy[] = R"(
  {
    "AC": {
      "Delays": {
        "ScreenDim": 5000,
        "ScreenOff": 7000,
        "Idle": 9000
      },
      "IdleAction": "DoNothing"
    },
    "Battery": {
      "Delays": {
        "ScreenDim": 10000,
        "ScreenOff": 3000,
        "Idle": 4000
      },
      "IdleAction": "DoNothing"
    },
    "LidCloseAction": "DoNothing",
    "UserActivityScreenDimDelayScale": 300
  }
)";

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
  const std::string kTestONC = R"(
    {
      "NetworkConfigurations": [
        {
          "GUID": "{485d6076-dd44-6b6d-69787465725f5045}",
          "Type": "WiFi",
          "Name": "some name",
          "WiFi": {
            "Security": "WEP-PSK",
            "SSID": "ssid",
            "Passphrase": "pass"
          }
        }
      ]
    }
  )";

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
  const std::string kTestONC = R"(
    {
      "NetworkConfigurations": [
        {
          "GUID": "{485d6076-dd44-6b6d-69787465725f5045}",
          "Type": "WiFi",
          "Name": "some name",
          "WiFi": {
            "Security": "WEP-PSK",
            "SSID": "ssid",
            "Passphrase": "pass"
          }
        }
      ]
    }
  )";

  PolicyMap policy_map;
  policy_map.Set(key::kOpenNetworkConfiguration, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(kTestONC),
                 nullptr);
  std::unique_ptr<NetworkConfigurationPolicyHandler> handler(
      NetworkConfigurationPolicyHandler::CreateForUserPolicy());
  PolicyErrorMap errors;
  handler->PrepareForDisplaying(&policy_map);
  const base::Value* sanitized = policy_map.GetValue(
      key::kOpenNetworkConfiguration, base::Value::Type::STRING);
  ASSERT_TRUE(sanitized);
  ASSERT_TRUE(sanitized->is_string());
  const std::string& sanitized_onc = sanitized->GetString();
  EXPECT_FALSE(sanitized_onc.empty());
  EXPECT_EQ(std::string::npos, sanitized_onc.find("pass"));
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

TEST_F(PowerManagementIdleSettingsPolicyHandlerTest,
       MinimumIdleWithoutChangingIdleAction) {
  const std::string policy_with_minimum_correct_idle_timeouts = R"(
    {
      "AC": {
        "IdleAction": "Shutdown",
        "Delays": {
          "Idle": 1
        }
      },
      "Battery": {
        "IdleAction": "Shutdown",
        "Delays": {
          "Idle": 1
        }
      }
    }
  )";
  policy_.Set(key::kPowerManagementIdleSettings, POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
              base::JSONReader::Read(policy_with_minimum_correct_idle_timeouts),
              nullptr);
  PowerManagementIdleSettingsPolicyHandler handler(chrome_schema_);
  handler.ApplyPolicySettings(policy_, &prefs_);

  EXPECT_EQ(base::Value(1), GetPref(&prefs_, ash::prefs::kPowerAcIdleDelayMs));
  EXPECT_EQ(base::Value(chromeos::PowerPolicyController::ACTION_SHUT_DOWN),
            GetPref(&prefs_, ash::prefs::kPowerAcIdleAction));
  EXPECT_EQ(base::Value(1),
            GetPref(&prefs_, ash::prefs::kPowerBatteryIdleDelayMs));
  EXPECT_EQ(base::Value(chromeos::PowerPolicyController::ACTION_SHUT_DOWN),
            GetPref(&prefs_, ash::prefs::kPowerBatteryIdleAction));
}

TEST_F(PowerManagementIdleSettingsPolicyHandlerTest,
       SetPowerAcIdleActionToDoNothingForZeroAcIdleDelay) {
  const std::string policy_with_zero_ac_idle = R"(
    {
      "AC": {
        "IdleAction": "Shutdown",
        "Delays": {
          "Idle": 0
        }
      },
      "Battery": {
        "IdleAction": "Shutdown",
        "Delays": {
          "Idle": 30000
        }
      }
    }
  )";
  policy_.Set(key::kPowerManagementIdleSettings, POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
              base::JSONReader::Read(policy_with_zero_ac_idle), nullptr);
  PowerManagementIdleSettingsPolicyHandler handler(chrome_schema_);
  handler.ApplyPolicySettings(policy_, &prefs_);

  EXPECT_EQ(base::Value(0), GetPref(&prefs_, ash::prefs::kPowerAcIdleDelayMs));
  EXPECT_EQ(base::Value(chromeos::PowerPolicyController::ACTION_DO_NOTHING),
            GetPref(&prefs_, ash::prefs::kPowerAcIdleAction));
  // Do not change battery idle action.
  EXPECT_EQ(base::Value(chromeos::PowerPolicyController::ACTION_SHUT_DOWN),
            GetPref(&prefs_, ash::prefs::kPowerBatteryIdleAction));
}

TEST_F(PowerManagementIdleSettingsPolicyHandlerTest,
       SetPowerBatteryIdleActionToDoNothingForZeroBatteryIdleDelay) {
  const std::string policy_with_zero_battery_idle = R"(
    {
      "AC": {
        "IdleAction": "Shutdown",
        "Delays": {
          "Idle": 30000
        }
      },
      "Battery": {
        "IdleAction": "Shutdown",
        "Delays": {
          "Idle": 0
        }
      }
    }
  )";
  policy_.Set(key::kPowerManagementIdleSettings, POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
              base::JSONReader::Read(policy_with_zero_battery_idle), nullptr);
  PowerManagementIdleSettingsPolicyHandler handler(chrome_schema_);
  handler.ApplyPolicySettings(policy_, &prefs_);

  EXPECT_EQ(base::Value(0),
            GetPref(&prefs_, ash::prefs::kPowerBatteryIdleDelayMs));
  EXPECT_EQ(base::Value(chromeos::PowerPolicyController::ACTION_DO_NOTHING),
            GetPref(&prefs_, ash::prefs::kPowerBatteryIdleAction));
  // Do not change AC idle action.
  EXPECT_EQ(base::Value(chromeos::PowerPolicyController::ACTION_SHUT_DOWN),
            GetPref(&prefs_, ash::prefs::kPowerAcIdleAction));
}

TEST_F(PowerManagementIdleSettingsPolicyHandlerTest,
       DoNotChangeIdleActionIfIdleTimeoutNotSet) {
  const std::string policy_without_idle_timeouts = R"(
    {
      "AC": {
        "IdleAction": "Shutdown"
      },
      "Battery": {
        "IdleAction": "Shutdown"
      }
    }
  )";
  policy_.Set(key::kPowerManagementIdleSettings, POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
              base::JSONReader::Read(policy_without_idle_timeouts), nullptr);
  PowerManagementIdleSettingsPolicyHandler handler(chrome_schema_);
  handler.ApplyPolicySettings(policy_, &prefs_);

  EXPECT_EQ(base::Value(chromeos::PowerPolicyController::ACTION_SHUT_DOWN),
            GetPref(&prefs_, ash::prefs::kPowerBatteryIdleAction));
  EXPECT_EQ(base::Value(chromeos::PowerPolicyController::ACTION_SHUT_DOWN),
            GetPref(&prefs_, ash::prefs::kPowerAcIdleAction));
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
  EXPECT_EQ(base::Value(false),
            GetPref(&prefs, arc::prefs::kArcBackupRestoreEnabled));
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
  EXPECT_EQ(base::Value(false),
            GetPref(&prefs, arc::prefs::kArcBackupRestoreEnabled));
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
  EXPECT_EQ(base::Value(true),
            GetPref(&prefs, arc::prefs::kArcBackupRestoreEnabled));
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
  EXPECT_EQ(base::Value(false),
            GetPref(&prefs, arc::prefs::kArcBackupRestoreEnabled));
}

TEST(ArcServicePolicyHandlerTest, UnderUserControlForConsumer) {
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

TEST_F(HelpMeWritePolicyHandlerTest, Default) {
  handler_.ApplyPolicySettings(policy_, &prefs_);

  EXPECT_FALSE(prefs_.GetValue(ash::prefs::kOrcaEnabled, nullptr));
  EXPECT_FALSE(prefs_.GetValue(ash::prefs::kOrcaFeedbackEnabled, nullptr));
}

TEST_F(HelpMeWritePolicyHandlerTest, EnabledWithModelImprovement) {
  policy_.Set(key::kHelpMeWriteSettings, POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(0), nullptr);
  handler_.ApplyPolicySettings(policy_, &prefs_);

  EXPECT_EQ(base::Value(true), GetPref(&prefs_, ash::prefs::kOrcaEnabled));
  EXPECT_EQ(base::Value(true),
            GetPref(&prefs_, ash::prefs::kOrcaFeedbackEnabled));
}

TEST_F(HelpMeWritePolicyHandlerTest, EnabledWithoutModelImprovement) {
  policy_.Set(key::kHelpMeWriteSettings, POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(1), nullptr);
  handler_.ApplyPolicySettings(policy_, &prefs_);

  EXPECT_EQ(base::Value(true), GetPref(&prefs_, ash::prefs::kOrcaEnabled));
  EXPECT_EQ(base::Value(false),
            GetPref(&prefs_, ash::prefs::kOrcaFeedbackEnabled));
}

TEST_F(HelpMeWritePolicyHandlerTest, Disabled) {
  policy_.Set(key::kHelpMeWriteSettings, POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(2), nullptr);
  handler_.ApplyPolicySettings(policy_, &prefs_);

  EXPECT_EQ(base::Value(false), GetPref(&prefs_, ash::prefs::kOrcaEnabled));
  EXPECT_EQ(base::Value(false),
            GetPref(&prefs_, ash::prefs::kOrcaFeedbackEnabled));
}

}  // namespace policy
