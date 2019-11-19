// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/configuration_policy_handler_chromeos.h"

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "ash/public/cpp/ash_pref_names.h"
#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/chromeos/accessibility/magnifier_type.h"
#include "chrome/browser/chromeos/arc/policy/arc_policy_util.h"
#include "chrome/browser/ui/ash/chrome_launcher_prefs.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/power/power_policy_controller.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_utils.h"
#include "chromeos/network/onc/onc_validator.h"
#include "components/arc/arc_prefs.h"
#include "components/crx_file/id_util.h"
#include "components/onc/onc_constants.h"
#include "components/onc/onc_pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"
#include "crypto/sha2.h"
#include "url/gurl.h"

namespace apu = arc::policy_util;

namespace policy {

namespace {

const char kSubkeyURL[] = "url";
const char kSubkeyHash[] = "hash";

base::Optional<std::string> GetSubkeyString(const base::Value& dict,
                                            policy::PolicyErrorMap* errors,
                                            const std::string& policy,
                                            const std::string& subkey) {
  const base::Value* policy_value = dict.FindKey(subkey);
  if (!policy_value) {
    errors->AddError(policy, subkey, IDS_POLICY_NOT_SPECIFIED_ERROR);
    return base::nullopt;
  }
  if (!policy_value->is_string()) {
    errors->AddError(policy, subkey, IDS_POLICY_TYPE_ERROR, "string");
    return base::nullopt;
  }
  if (policy_value->GetString().empty()) {
    errors->AddError(policy, subkey, IDS_POLICY_NOT_SPECIFIED_ERROR);
    return base::nullopt;
  }
  return policy_value->GetString();
}

constexpr base::StringPiece kScreenDimDelayAC[] = {"AC", "Delays", "ScreenDim"};
constexpr base::StringPiece kScreenOffDelayAC[] = {"AC", "Delays", "ScreenOff"};
constexpr base::StringPiece kIdleWarningDelayAC[] = {"AC", "Delays",
                                                     "IdleWarning"};
constexpr base::StringPiece kIdleDelayAC[] = {"AC", "Delays", "Idle"};
constexpr base::StringPiece kIdleActionAC[] = {"AC", "IdleAction"};
constexpr base::StringPiece kScreenDimDelayBattery[] = {"Battery", "Delays",
                                                        "ScreenDim"};
constexpr base::StringPiece kScreenOffDelayBattery[] = {"Battery", "Delays",
                                                        "ScreenOff"};
constexpr base::StringPiece kIdleWarningDelayBattery[] = {"Battery", "Delays",
                                                          "IdleWarning"};
constexpr base::StringPiece kIdleDelayBattery[] = {"Battery", "Delays", "Idle"};
constexpr base::StringPiece kIdleActionBattery[] = {"Battery", "IdleAction"};

constexpr char kScreenLockDelayAC[] = "AC";
const char kScreenLockDelayBattery[] = "Battery";

constexpr char kActionSuspend[] = "Suspend";
constexpr char kActionLogout[] = "Logout";
constexpr char kActionShutdown[] = "Shutdown";
constexpr char kActionDoNothing[] = "DoNothing";

constexpr char kScreenBrightnessPercentAC[] = "BrightnessAC";
constexpr char kScreenBrightnessPercentBattery[] = "BrightnessBattery";

// Converts the string held by |value| to an int Value holding the corresponding
// |chromeos::PowerPolicyController| enum value. Returns an empty value if
// |value| is nullptr, not a string or if |value| holds a string which does not
// represent a known action.
base::Value ConvertToActionEnumValue(const base::Value* value) {
  if (!value || !value->is_string())
    return base::Value();
  if (value->GetString() == kActionSuspend)
    return base::Value(chromeos::PowerPolicyController::ACTION_SUSPEND);
  if (value->GetString() == kActionLogout)
    return base::Value(chromeos::PowerPolicyController::ACTION_STOP_SESSION);
  if (value->GetString() == kActionShutdown)
    return base::Value(chromeos::PowerPolicyController::ACTION_SHUT_DOWN);
  if (value->GetString() == kActionDoNothing)
    return base::Value(chromeos::PowerPolicyController::ACTION_DO_NOTHING);
  return base::Value();
}

}  // namespace

ExternalDataPolicyHandler::ExternalDataPolicyHandler(const char* policy_name)
    : TypeCheckingPolicyHandler(policy_name, base::Value::Type::DICTIONARY) {}

ExternalDataPolicyHandler::~ExternalDataPolicyHandler() {
}

bool ExternalDataPolicyHandler::CheckPolicySettings(const PolicyMap& policies,
                                                    PolicyErrorMap* errors) {
  if (!TypeCheckingPolicyHandler::CheckPolicySettings(policies, errors))
    return false;

  const std::string policy = policy_name();
  const base::Value* value = policies.GetValue(policy);
  if (!value)
    return true;
  if (!value->is_dict()) {
    NOTREACHED();
    return false;
  }
  base::Optional<std::string> url_string =
      GetSubkeyString(*value, errors, policy, kSubkeyURL);
  base::Optional<std::string> hash_string =
      GetSubkeyString(*value, errors, policy, kSubkeyHash);
  if (!url_string || !hash_string)
    return false;

  const GURL url(url_string.value());
  if (!url.is_valid()) {
    errors->AddError(policy, kSubkeyURL, IDS_POLICY_VALUE_FORMAT_ERROR);
    return false;
  }

  std::vector<uint8_t> hash;
  if (!base::HexStringToBytes(hash_string.value(), &hash) ||
      hash.size() != crypto::kSHA256Length) {
    errors->AddError(policy, kSubkeyHash, IDS_POLICY_VALUE_FORMAT_ERROR);
    return false;
  }

  return true;
}

void ExternalDataPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                    PrefValueMap* prefs) {
}

// static
NetworkConfigurationPolicyHandler*
NetworkConfigurationPolicyHandler::CreateForUserPolicy() {
  return new NetworkConfigurationPolicyHandler(
      key::kOpenNetworkConfiguration, onc::ONC_SOURCE_USER_POLICY,
      onc::prefs::kOpenNetworkConfiguration);
}

// static
NetworkConfigurationPolicyHandler*
NetworkConfigurationPolicyHandler::CreateForDevicePolicy() {
  return new NetworkConfigurationPolicyHandler(
      key::kDeviceOpenNetworkConfiguration, onc::ONC_SOURCE_DEVICE_POLICY,
      onc::prefs::kDeviceOpenNetworkConfiguration);
}

NetworkConfigurationPolicyHandler::~NetworkConfigurationPolicyHandler() {}

bool NetworkConfigurationPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  const base::Value* value;
  if (!CheckAndGetValue(policies, errors, &value))
    return false;

  if (!value)
    return true;

  std::unique_ptr<base::Value> root_dict =
      chromeos::onc::ReadDictionaryFromJson(value->GetString());
  if (!root_dict) {
    errors->AddError(policy_name(), IDS_POLICY_NETWORK_CONFIG_PARSE_FAILED);
    errors->SetDebugInfo(policy_name(), "ERROR: JSON parse error");
    return false;
  }

  // Validate the ONC dictionary. We are liberal and ignore unknown field
  // names and ignore invalid field names in kRecommended arrays.
  chromeos::onc::Validator validator(
      false,  // Ignore unknown fields.
      false,  // Ignore invalid recommended field names.
      true,   // Fail on missing fields.
      true,   // Validate for managed ONC.
      true);  // Log warnings.
  validator.SetOncSource(onc_source_);

  // ONC policies are always unencrypted.
  chromeos::onc::Validator::Result validation_result;
  root_dict = validator.ValidateAndRepairObject(
      &chromeos::onc::kToplevelConfigurationSignature, *root_dict,
      &validation_result);

  // Pass error/warning message and non-localized debug_info to PolicyErrorMap.
  std::vector<base::StringPiece> messages;
  for (const chromeos::onc::Validator::ValidationIssue& issue :
       validator.validation_issues()) {
    messages.push_back(issue.message);
  }
  std::string debug_info = base::JoinString(messages, "\n");

  if (validation_result == chromeos::onc::Validator::VALID_WITH_WARNINGS)
    errors->AddError(policy_name(), IDS_POLICY_NETWORK_CONFIG_IMPORT_PARTIAL,
                     debug_info);
  else if (validation_result == chromeos::onc::Validator::INVALID)
    errors->AddError(policy_name(), IDS_POLICY_NETWORK_CONFIG_IMPORT_FAILED,
                     debug_info);

  if (!validator.validation_issues().empty())
    errors->SetDebugInfo(policy_name(), debug_info);

  // In any case, don't reject the policy as some networks or certificates could
  // still be applied.

  return true;
}

void NetworkConfigurationPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* value = policies.GetValue(policy_name());
  if (!value)
    return;

  const std::string& onc_blob = value->GetString();

  base::ListValue network_configs;
  base::ListValue certificates;
  base::DictionaryValue global_network_config;
  chromeos::onc::ParseAndValidateOncForImport(
      onc_blob, onc_source_, "", &network_configs, &global_network_config,
      &certificates);

  // Currently, only the per-network configuration is stored in a pref. Ignore
  // |global_network_config| and |certificates|.
  prefs->SetValue(pref_path_, std::move(network_configs));
}

void NetworkConfigurationPolicyHandler::PrepareForDisplaying(
    PolicyMap* policies) const {
  const PolicyMap::Entry* entry = policies->Get(policy_name());
  if (!entry)
    return;
  std::unique_ptr<base::Value> sanitized_config =
      SanitizeNetworkConfig(entry->value.get());
  if (!sanitized_config)
    sanitized_config = std::make_unique<base::Value>();

  policies->Set(policy_name(), entry->level, entry->scope, entry->source,
                std::move(sanitized_config), nullptr);
}

NetworkConfigurationPolicyHandler::NetworkConfigurationPolicyHandler(
    const char* policy_name,
    onc::ONCSource onc_source,
    const char* pref_path)
    : TypeCheckingPolicyHandler(policy_name, base::Value::Type::STRING),
      onc_source_(onc_source),
      pref_path_(pref_path) {}

// static
std::unique_ptr<base::Value>
NetworkConfigurationPolicyHandler::SanitizeNetworkConfig(
    const base::Value* config) {
  if (!config->is_string())
    return nullptr;

  std::unique_ptr<base::DictionaryValue> toplevel_dict =
      base::DictionaryValue::From(
          chromeos::onc::ReadDictionaryFromJson(config->GetString()));
  if (!toplevel_dict)
    return nullptr;

  // Placeholder to insert in place of the filtered setting.
  const char kPlaceholder[] = "********";

  toplevel_dict = chromeos::onc::MaskCredentialsInOncObject(
      chromeos::onc::kToplevelConfigurationSignature,
      *toplevel_dict,
      kPlaceholder);

  std::string json_string;
  base::JSONWriter::WriteWithOptions(
      *toplevel_dict, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json_string);
  return std::make_unique<base::Value>(json_string);
}

PinnedLauncherAppsPolicyHandler::PinnedLauncherAppsPolicyHandler()
    : ListPolicyHandler(key::kPinnedLauncherApps, base::Value::Type::STRING) {}

PinnedLauncherAppsPolicyHandler::~PinnedLauncherAppsPolicyHandler() {}

bool PinnedLauncherAppsPolicyHandler::CheckListEntry(const base::Value& value) {
  // Assume it's an Android app if it contains a dot.
  const std::string& str = value.GetString();
  if (str.find(".") != std::string::npos)
    return true;

  // Otherwise, check if it's an extension id.
  return crx_file::id_util::IdIsValid(str);
}

void PinnedLauncherAppsPolicyHandler::ApplyList(
    std::unique_ptr<base::ListValue> filtered_list,
    PrefValueMap* prefs) {
  std::vector<base::Value> pinned_apps_list;
  for (const base::Value& entry : filtered_list->GetList()) {
    base::Value app_dict(base::Value::Type::DICTIONARY);
    app_dict.SetKey(kPinnedAppsPrefAppIDKey, entry.Clone());
    pinned_apps_list.push_back(std::move(app_dict));
  }
  prefs->SetValue(prefs::kPolicyPinnedLauncherApps,
                  base::Value(std::move(pinned_apps_list)));
}

ScreenMagnifierPolicyHandler::ScreenMagnifierPolicyHandler()
    : IntRangePolicyHandlerBase(key::kScreenMagnifierType,
                                chromeos::MAGNIFIER_DISABLED,
                                chromeos::MAGNIFIER_DOCKED,
                                false) {}

ScreenMagnifierPolicyHandler::~ScreenMagnifierPolicyHandler() {
}

void ScreenMagnifierPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* value = policies.GetValue(policy_name());
  int value_in_range;
  if (value && EnsureInRange(value, &value_in_range, nullptr)) {
    prefs->SetBoolean(ash::prefs::kAccessibilityScreenMagnifierEnabled,
                      value_in_range == chromeos::MAGNIFIER_FULL);
    prefs->SetBoolean(ash::prefs::kDockedMagnifierEnabled,
                      value_in_range == chromeos::MAGNIFIER_DOCKED);
  }
}

LoginScreenPowerManagementPolicyHandler::
    LoginScreenPowerManagementPolicyHandler(const Schema& chrome_schema)
    : SchemaValidatingPolicyHandler(key::kDeviceLoginScreenPowerManagement,
                                    chrome_schema.GetKnownProperty(
                                        key::kDeviceLoginScreenPowerManagement),
                                    SCHEMA_ALLOW_UNKNOWN) {
}

LoginScreenPowerManagementPolicyHandler::
    ~LoginScreenPowerManagementPolicyHandler() {
}

void LoginScreenPowerManagementPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
}

DeprecatedIdleActionHandler::DeprecatedIdleActionHandler()
    : IntRangePolicyHandlerBase(
          key::kIdleAction,
          chromeos::PowerPolicyController::ACTION_SUSPEND,
          chromeos::PowerPolicyController::ACTION_DO_NOTHING,
          false) {}

DeprecatedIdleActionHandler::~DeprecatedIdleActionHandler() {}

void DeprecatedIdleActionHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                      PrefValueMap* prefs) {
  const base::Value* value = policies.GetValue(policy_name());
  if (value && EnsureInRange(value, nullptr, nullptr)) {
    if (!prefs->GetValue(ash::prefs::kPowerAcIdleAction, nullptr))
      prefs->SetValue(ash::prefs::kPowerAcIdleAction, value->Clone());
    if (!prefs->GetValue(ash::prefs::kPowerBatteryIdleAction, nullptr)) {
      prefs->SetValue(ash::prefs::kPowerBatteryIdleAction, value->Clone());
    }
  }
}

PowerManagementIdleSettingsPolicyHandler::
    PowerManagementIdleSettingsPolicyHandler(const Schema& chrome_schema)
    : SchemaValidatingPolicyHandler(
          key::kPowerManagementIdleSettings,
          chrome_schema.GetKnownProperty(key::kPowerManagementIdleSettings),
          SCHEMA_ALLOW_UNKNOWN) {
}

PowerManagementIdleSettingsPolicyHandler::
    ~PowerManagementIdleSettingsPolicyHandler() {
}

void PowerManagementIdleSettingsPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  std::unique_ptr<base::Value> policy_value;
  if (!CheckAndGetValue(policies, nullptr, &policy_value) || !policy_value)
    return;
  DCHECK(policy_value->is_dict());

  const base::Value* value;
  base::Value action_value;

  value = policy_value->FindPath(kScreenDimDelayAC);
  if (value)
    prefs->SetValue(ash::prefs::kPowerAcScreenDimDelayMs, value->Clone());

  value = policy_value->FindPath(kScreenOffDelayAC);
  if (value)
    prefs->SetValue(ash::prefs::kPowerAcScreenOffDelayMs, value->Clone());

  value = policy_value->FindPath(kIdleWarningDelayAC);
  if (value)
    prefs->SetValue(ash::prefs::kPowerAcIdleWarningDelayMs, value->Clone());

  value = policy_value->FindPath(kIdleDelayAC);
  if (value)
    prefs->SetValue(ash::prefs::kPowerAcIdleDelayMs, value->Clone());

  action_value =
      ConvertToActionEnumValue(policy_value->FindPath(kIdleActionAC));
  if (!action_value.is_none())
    prefs->SetValue(ash::prefs::kPowerAcIdleAction, std::move(action_value));

  value = policy_value->FindPath(kScreenDimDelayBattery);
  if (value)
    prefs->SetValue(ash::prefs::kPowerBatteryScreenDimDelayMs, value->Clone());

  value = policy_value->FindPath(kScreenOffDelayBattery);
  if (value)
    prefs->SetValue(ash::prefs::kPowerBatteryScreenOffDelayMs, value->Clone());

  value = policy_value->FindPath(kIdleWarningDelayBattery);
  if (value) {
    prefs->SetValue(ash::prefs::kPowerBatteryIdleWarningDelayMs,
                    value->Clone());
  }

  value = policy_value->FindPath(kIdleDelayBattery);
  if (value)
    prefs->SetValue(ash::prefs::kPowerBatteryIdleDelayMs, value->Clone());

  action_value =
      ConvertToActionEnumValue(policy_value->FindPath(kIdleActionBattery));
  if (!action_value.is_none()) {
    prefs->SetValue(ash::prefs::kPowerBatteryIdleAction,
                    std::move(action_value));
  }
}

ScreenLockDelayPolicyHandler::ScreenLockDelayPolicyHandler(
    const Schema& chrome_schema)
    : SchemaValidatingPolicyHandler(
          key::kScreenLockDelays,
          chrome_schema.GetKnownProperty(key::kScreenLockDelays),
          SCHEMA_ALLOW_UNKNOWN) {
}

ScreenLockDelayPolicyHandler::~ScreenLockDelayPolicyHandler() {
}

void ScreenLockDelayPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  std::unique_ptr<base::Value> policy_value;
  if (!CheckAndGetValue(policies, nullptr, &policy_value) || !policy_value)
    return;
  DCHECK(policy_value->is_dict());

  const base::Value* value;
  value = policy_value->FindKey(kScreenLockDelayAC);
  if (value)
    prefs->SetValue(ash::prefs::kPowerAcScreenLockDelayMs, value->Clone());

  value = policy_value->FindKey(kScreenLockDelayBattery);
  if (value)
    prefs->SetValue(ash::prefs::kPowerBatteryScreenLockDelayMs, value->Clone());
}

ScreenBrightnessPercentPolicyHandler::ScreenBrightnessPercentPolicyHandler(
    const Schema& chrome_schema)
    : SchemaValidatingPolicyHandler(
          key::kScreenBrightnessPercent,
          chrome_schema.GetKnownProperty(key::kScreenBrightnessPercent),
          SCHEMA_ALLOW_UNKNOWN) {}

ScreenBrightnessPercentPolicyHandler::~ScreenBrightnessPercentPolicyHandler() =
    default;

void ScreenBrightnessPercentPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  std::unique_ptr<base::Value> policy_value;
  if (!CheckAndGetValue(policies, nullptr, &policy_value) || !policy_value)
    return;
  DCHECK(policy_value->is_dict());

  const base::Value* value = policy_value->FindKey(kScreenBrightnessPercentAC);
  if (value) {
    prefs->SetValue(ash::prefs::kPowerAcScreenBrightnessPercent,
                    value->Clone());
  }
  value = policy_value->FindKey(kScreenBrightnessPercentBattery);
  if (value) {
    prefs->SetValue(ash::prefs::kPowerBatteryScreenBrightnessPercent,
                    value->Clone());
  }
}

ArcServicePolicyHandler::ArcServicePolicyHandler(const char* policy,
                                                 const char* pref)
    : IntRangePolicyHandlerBase(
          policy,
          static_cast<int>(ArcServicePolicyValue::kDisabled),
          static_cast<int>(ArcServicePolicyValue::kEnabled),
          false /* clamp */),
      pref_(pref) {}

void ArcServicePolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                  PrefValueMap* prefs) {
  const base::Value* const value = policies.GetValue(policy_name());
  if (!value) {
    return;
  }
  const base::Value* current_value = nullptr;
  if (prefs->GetValue(pref_, &current_value)) {
    // If a value for this policy was already set by another handler, do not
    // clobber it. This is necessary so that the DefaultGeolocationSetting
    // policy can take precedence over ArcLocationServiceEnabled.
    return;
  }
  if (value->GetInt() == static_cast<int>(ArcServicePolicyValue::kDisabled)) {
    prefs->SetBoolean(pref_, false);
  } else if (value->GetInt() ==
             static_cast<int>(ArcServicePolicyValue::kEnabled)) {
    prefs->SetBoolean(pref_, true);
  }
}

EcryptfsMigrationStrategyPolicyHandler::EcryptfsMigrationStrategyPolicyHandler()
    : IntRangePolicyHandlerBase(
          key::kEcryptfsMigrationStrategy,
          static_cast<int>(apu::EcryptfsMigrationAction::kDisallowMigration),
          static_cast<int>(apu::EcryptfsMigrationAction::
                               kAskForEcryptfsArcUsersNoLongerSupported),
          false /* clamp */) {}

void EcryptfsMigrationStrategyPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* const value = policies.GetValue(policy_name());
  if (!value || !EnsureInRange(value, nullptr, nullptr)) {
    return;
  }
  if (value->GetInt() ==
          static_cast<int>(apu::EcryptfsMigrationAction::kAskUser) ||
      value->GetInt() ==
          static_cast<int>(apu::EcryptfsMigrationAction::
                               kAskForEcryptfsArcUsersNoLongerSupported)) {
    // Alias obsolete values to apu::kMigrate.
    prefs->SetInteger(arc::prefs::kEcryptfsMigrationStrategy,
                      static_cast<int>(apu::EcryptfsMigrationAction::kMigrate));
  } else {
    prefs->SetValue(arc::prefs::kEcryptfsMigrationStrategy, value->Clone());
  }
}

}  // namespace policy
