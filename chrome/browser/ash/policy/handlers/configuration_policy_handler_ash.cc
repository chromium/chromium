// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/configuration_policy_handler_ash.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "ash/components/arc/arc_prefs.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/geolocation_access_level.h"
#include "ash/system/privacy_hub/privacy_hub_controller.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/policy_util.h"
#include "chrome/browser/ash/accessibility/magnifier_type.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_prefs.h"
#include "chrome/common/pref_names.h"
#include "chromeos/components/onc/onc_signature.h"
#include "chromeos/components/onc/onc_utils.h"
#include "chromeos/components/onc/onc_validator.h"
#include "chromeos/dbus/power/power_policy_controller.h"
#include "components/crx_file/id_util.h"
#include "components/onc/onc_constants.h"
#include "components/onc/onc_pref_names.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"
#include "crypto/sha2.h"
#include "url/gurl.h"

namespace policy {

namespace {

using ::ash::MagnifierType;

constexpr char kPolicyEntryPolicyIdKey[] = "policy_id";
constexpr char kPolicyEntryFileExtensionsKey[] = "file_extensions";

constexpr char kSubkeyURL[] = "url";
constexpr char kSubkeyHash[] = "hash";

std::optional<std::string> GetSubkeyString(const base::Value::Dict& dict,
                                           PolicyErrorMap* errors,
                                           const std::string& policy,
                                           const std::string& subkey) {
  const base::Value* policy_value = dict.Find(subkey);

  if (!policy_value) {
    errors->AddError(policy, IDS_POLICY_NOT_SPECIFIED_ERROR,
                     PolicyErrorPath{subkey});
    return std::nullopt;
  }
  if (!policy_value->is_string()) {
    errors->AddError(policy, IDS_POLICY_TYPE_ERROR,
                     base::Value::GetTypeName(base::Value::Type::STRING),
                     PolicyErrorPath{subkey});
    return std::nullopt;
  }
  if (policy_value->GetString().empty()) {
    errors->AddError(policy, IDS_POLICY_NOT_SPECIFIED_ERROR,
                     PolicyErrorPath{subkey});
    return std::nullopt;
  }
  return policy_value->GetString();
}

constexpr char kScreenDimDelayAC[] = "AC.Delays.ScreenDim";
constexpr char kScreenOffDelayAC[] = "AC.Delays.ScreenOff";
constexpr char kIdleWarningDelayAC[] = "AC.Delays.IdleWarning";
constexpr char kIdleDelayAC[] = "AC.Delays.Idle";
constexpr char kIdleActionAC[] = "AC.IdleAction";
constexpr char kScreenDimDelayBattery[] = "Battery.Delays.ScreenDim";
constexpr char kScreenOffDelayBattery[] = "Battery.Delays.ScreenOff";
constexpr char kIdleWarningDelayBattery[] = "Battery.Delays.IdleWarning";
constexpr char kIdleDelayBattery[] = "Battery.Delays.Idle";
constexpr char kIdleActionBattery[] = "Battery.IdleAction";

constexpr char kScreenLockDelayAC[] = "AC";
constexpr char kScreenLockDelayBattery[] = "Battery";

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
  if (!value || !value->is_string()) {
    return base::Value();
  }
  if (value->GetString() == kActionSuspend) {
    return base::Value(chromeos::PowerPolicyController::ACTION_SUSPEND);
  }
  if (value->GetString() == kActionLogout) {
    return base::Value(chromeos::PowerPolicyController::ACTION_STOP_SESSION);
  }
  if (value->GetString() == kActionShutdown) {
    return base::Value(chromeos::PowerPolicyController::ACTION_SHUT_DOWN);
  }
  if (value->GetString() == kActionDoNothing) {
    return base::Value(chromeos::PowerPolicyController::ACTION_DO_NOTHING);
  }
  return base::Value();
}

void SetPrefValueIfNotNull(PrefValueMap* prefs,
                           const std::string& name,
                           const base::Value* value) {
  if (value) {
    prefs->SetValue(name, value->Clone());
  }
}

base::Value CalculateIdleActionValue(const base::Value* idle_action_value,
                                     const base::Value* idle_delay_value) {
  // From the PowerManagementIdleSettings policy description, zero value for
  // the idle delay should disable the idle action. But for prefs, setting
  // |kPowerAcIdleDelayMs| or |kPowerBatteryIdleDelayMs| to zero does not
  // disable the corresponding idle action. See b/202113291. To be consistent
  // with policy description, we set power idle action to |ACTION_DO_NOTHING|,
  // if the idle delay is zero.
  if (idle_delay_value && idle_delay_value->GetInt() == 0) {
    return base::Value(chromeos::PowerPolicyController::ACTION_DO_NOTHING);
  }
  return ConvertToActionEnumValue(idle_action_value);
}

bool IsSupportedAppTypePolicyId(std::string_view policy_id) {
  return apps_util::IsChromeAppPolicyId(policy_id) ||
         apps_util::IsArcAppPolicyId(policy_id) ||
         apps_util::IsSystemWebAppPolicyId(policy_id) ||
         apps_util::IsWebAppPolicyId(policy_id) ||
         apps_util::IsPreinstalledWebAppPolicyId(policy_id) ||
         apps_util::IsIsolatedWebAppPolicyId(policy_id);
}

}  // namespace

ExternalDataPolicyHandler::ExternalDataPolicyHandler(const char* policy_name)
    : TypeCheckingPolicyHandler(policy_name, base::Value::Type::DICT) {}

ExternalDataPolicyHandler::~ExternalDataPolicyHandler() {}

bool ExternalDataPolicyHandler::CheckPolicySettings(const PolicyMap& policies,
                                                    PolicyErrorMap* errors) {
  const std::string policy = policy_name();
  if (!policies.IsPolicySet(policy)) {
    return true;
  }

  return CheckPolicySettings(policy.c_str(), policies.Get(policy), errors);
}

bool ExternalDataPolicyHandler::CheckPolicySettings(
    const char* policy,
    const PolicyMap::Entry* entry,
    PolicyErrorMap* errors) {
  if (!TypeCheckingPolicyHandler::CheckPolicySettings(
          policy, base::Value::Type::DICT, entry, errors)) {
    return false;
  }

  const base::Value* value = entry->value(base::Value::Type::DICT);
  DCHECK(value);
  const base::Value::Dict& dict = value->GetDict();
  std::optional<std::string> url_string =
      GetSubkeyString(dict, errors, policy, kSubkeyURL);
  std::optional<std::string> hash_string =
      GetSubkeyString(dict, errors, policy, kSubkeyHash);
  if (!url_string || !hash_string) {
    return false;
  }

  const GURL url(url_string.value());
  if (!url.is_valid()) {
    errors->AddError(policy, IDS_POLICY_INVALID_URL_ERROR,
                     PolicyErrorPath{kSubkeyURL});
    return false;
  }

  std::vector<uint8_t> hash;
  if (!base::HexStringToBytes(hash_string.value(), &hash) ||
      hash.size() != crypto::kSHA256Length) {
    errors->AddError(policy, IDS_POLICY_INVALID_HASH_ERROR,
                     PolicyErrorPath{kSubkeyHash});
    return false;
  }

  return true;
}

void ExternalDataPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                    PrefValueMap* prefs) {}

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
  if (!CheckAndGetValue(policies, errors, &value)) {
    return false;
  }

  if (!value) {
    return true;
  }

  std::optional<base::Value::Dict> root_dict =
      chromeos::onc::ReadDictionaryFromJson(value->GetString());
  if (!root_dict.has_value()) {
    errors->AddError(policy_name(), IDS_POLICY_NETWORK_CONFIG_PARSE_FAILED);
    return false;
  }

  // Validate the ONC dictionary. We are liberal and ignore unknown field
  // names and ignore invalid field names in kRecommended arrays.
  chromeos::onc::Validator validator(
      /*error_on_unknown_field=*/false,
      /*error_on_wrong_recommended=*/false,
      /*error_on_missing_field=*/true,
      /*managed_onc=*/true,
      /*log_warnings=*/true);
  validator.SetOncSource(onc_source_);

  // ONC policies are always unencrypted.
  chromeos::onc::Validator::Result validation_result;
  validator.ValidateAndRepairObject(
      &chromeos::onc::kToplevelConfigurationSignature, root_dict.value(),
      &validation_result);

  // Pass error/warning message and non-localized debug_info to PolicyErrorMap.
  std::vector<std::string_view> messages;
  for (const chromeos::onc::Validator::ValidationIssue& issue :
       validator.validation_issues()) {
    messages.push_back(issue.message);
  }
  std::string debug_info = base::JoinString(messages, "\n");

  if (validation_result == chromeos::onc::Validator::VALID_WITH_WARNINGS) {
    errors->AddError(policy_name(), IDS_POLICY_NETWORK_CONFIG_IMPORT_PARTIAL,
                     debug_info);
  } else if (validation_result == chromeos::onc::Validator::INVALID) {
    errors->AddError(policy_name(), IDS_POLICY_NETWORK_CONFIG_IMPORT_FAILED,
                     debug_info);
  }

  // In any case, don't reject the policy as some networks or certificates could
  // still be applied.
  return true;
}

void NetworkConfigurationPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::STRING);
  if (!value) {
    return;
  }

  const std::string& onc_blob = value->GetString();

  base::Value::List network_configs;
  base::Value::List certificates;
  base::Value::Dict global_network_config;
  chromeos::onc::ParseAndValidateOncForImport(
      onc_blob, onc_source_, "", &network_configs, &global_network_config,
      &certificates);

  // Currently, only the per-network configuration is stored in a pref. Ignore
  // |global_network_config| and |certificates|.
  prefs->SetValue(pref_path_, base::Value(std::move(network_configs)));
}

void NetworkConfigurationPolicyHandler::PrepareForDisplaying(
    PolicyMap* policies) const {
  const PolicyMap::Entry* entry = policies->Get(policy_name());
  if (!entry) {
    return;
  }
  std::optional<base::Value> sanitized_config =
      SanitizeNetworkConfig(entry->value(base::Value::Type::STRING));

  if (!sanitized_config.has_value()) {
    sanitized_config = base::Value();
  }

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
std::optional<base::Value>
NetworkConfigurationPolicyHandler::SanitizeNetworkConfig(
    const base::Value* config) {
  if (!config) {
    return std::nullopt;
  }

  std::optional<base::Value::Dict> config_dict =
      chromeos::onc::ReadDictionaryFromJson(config->GetString());
  if (!config_dict.has_value()) {
    return std::nullopt;
  }

  // Placeholder to insert in place of the filtered setting.
  const char kPlaceholder[] = "********";

  base::Value::Dict toplevel_dict = chromeos::onc::MaskCredentialsInOncObject(
      chromeos::onc::kToplevelConfigurationSignature, config_dict.value(),
      kPlaceholder);

  std::string json_string;
  base::JSONWriter::WriteWithOptions(
      toplevel_dict, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json_string);
  return base::Value(json_string);
}

PinnedLauncherAppsPolicyHandler::PinnedLauncherAppsPolicyHandler()
    : ListPolicyHandler(key::kPinnedLauncherApps, base::Value::Type::STRING) {}

PinnedLauncherAppsPolicyHandler::~PinnedLauncherAppsPolicyHandler() = default;

bool PinnedLauncherAppsPolicyHandler::CheckListEntry(const base::Value& value) {
  const std::string& policy_id = value.GetString();
  return IsSupportedAppTypePolicyId(policy_id);
}

void PinnedLauncherAppsPolicyHandler::ApplyList(base::Value::List filtered_list,
                                                PrefValueMap* prefs) {
  base::Value::List pinned_apps_list;
  for (base::Value& entry : filtered_list) {
    auto app_dict = base::Value::Dict().Set(
        ChromeShelfPrefs::kPinnedAppsPrefAppIDKey, std::move(entry));
    pinned_apps_list.Append(std::move(app_dict));
  }
  prefs->SetValue(prefs::kPolicyPinnedLauncherApps,
                  base::Value(std::move(pinned_apps_list)));
}

DefaultHandlersForFileExtensionsPolicyHandler::
    DefaultHandlersForFileExtensionsPolicyHandler(
        const policy::Schema& chrome_schema)
    : SchemaValidatingPolicyHandler(
          key::kDefaultHandlersForFileExtensions,
          chrome_schema.GetKnownProperty(
              key::kDefaultHandlersForFileExtensions),
          policy::SCHEMA_ALLOW_UNKNOWN_AND_INVALID_LIST_ENTRY) {}

// Verifies that each file extension is handler by no more that one app.
bool DefaultHandlersForFileExtensionsPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  std::unique_ptr<base::Value> policy_value;
  if (!CheckAndGetValue(policies, errors, &policy_value) || !policy_value) {
    return false;
  }

  base::flat_map<std::string, std::string> file_extension_to_policy_id;

  const auto& list = policy_value->GetList();
  for (uint32_t index = 0; index < list.size(); index++) {
    const auto& policy_entry_dict = list[index].GetDict();

    const std::string* policy_id =
        policy_entry_dict.FindString(kPolicyEntryPolicyIdKey);
    DCHECK(policy_id);

    if (!IsValidPolicyId(*policy_id)) {
      errors->AddError(policy_name(), IDS_POLICY_VALUE_FORMAT_ERROR,
                       PolicyErrorPath{index, kPolicyEntryPolicyIdKey});
      continue;
    }

    const auto* file_extensions =
        policy_entry_dict.FindList(kPolicyEntryFileExtensionsKey);
    DCHECK(file_extensions);

    for (const auto& file_extension_entry : *file_extensions) {
      const std::string& file_extension = file_extension_entry.GetString();

      if (auto it = file_extension_to_policy_id.find(file_extension);
          it != file_extension_to_policy_id.end()) {
        errors->AddError(
            policy_name(), IDS_POLICY_DUPLICATE_FILE_EXTENSION_ERROR,
            /*replacement_a=*/file_extension,
            /*replacement_b=*/base::JoinString({*policy_id, it->second}, ", "),
            /*error_path=*/{},
            /*error_level=*/PolicyMap::MessageType::kWarning);
        continue;
      }

      file_extension_to_policy_id[file_extension] = *policy_id;
    }
  }

  return true;
}

// Applies an inverse mapping to `prefs::kDefaultHandlersForFileExtensions`:
// file_extension -> id.
void DefaultHandlersForFileExtensionsPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  std::unique_ptr<base::Value> policy_value;
  CheckAndGetValue(policies, nullptr, &policy_value);

  base::Value::Dict pref_mapping;
  for (const auto& policy_entry : policy_value->GetList()) {
    const auto& policy_entry_dict = policy_entry.GetDict();

    const std::string* policy_id =
        policy_entry_dict.FindString(kPolicyEntryPolicyIdKey);
    if (!IsValidPolicyId(*policy_id)) {
      continue;
    }

    const auto* file_extensions =
        policy_entry_dict.FindList(kPolicyEntryFileExtensionsKey);

    for (const auto& file_extension_entry : *file_extensions) {
      pref_mapping.Set(base::StrCat({".", file_extension_entry.GetString()}),
                       *policy_id);
    }
  }

  prefs->SetValue(prefs::kDefaultHandlersForFileExtensions,
                  base::Value(std::move(pref_mapping)));
}

bool DefaultHandlersForFileExtensionsPolicyHandler::IsValidPolicyId(
    std::string_view policy_id) const {
  return IsSupportedAppTypePolicyId(policy_id) ||
         apps_util::IsFileManagerVirtualTaskPolicyId(policy_id);
}

ScreenMagnifierPolicyHandler::ScreenMagnifierPolicyHandler()
    : IntRangePolicyHandlerBase(key::kScreenMagnifierType,
                                static_cast<int>(MagnifierType::kDisabled),
                                static_cast<int>(MagnifierType::kDocked),
                                false) {}

ScreenMagnifierPolicyHandler::~ScreenMagnifierPolicyHandler() {}

void ScreenMagnifierPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  // It is safe to use `GetValueUnsafe()` because type checking is performed
  // before the value is used.
  const base::Value* value = policies.GetValueUnsafe(policy_name());
  int value_in_range;
  if (value && EnsureInRange(value, &value_in_range, nullptr)) {
    prefs->SetBoolean(ash::prefs::kAccessibilityScreenMagnifierEnabled,
                      value_in_range == static_cast<int>(MagnifierType::kFull));
    prefs->SetBoolean(
        ash::prefs::kDockedMagnifierEnabled,
        value_in_range == static_cast<int>(MagnifierType::kDocked));
  }
}

LoginScreenPowerManagementPolicyHandler::
    LoginScreenPowerManagementPolicyHandler(const Schema& chrome_schema)
    : SchemaValidatingPolicyHandler(key::kDeviceLoginScreenPowerManagement,
                                    chrome_schema.GetKnownProperty(
                                        key::kDeviceLoginScreenPowerManagement),
                                    SCHEMA_ALLOW_UNKNOWN) {}

LoginScreenPowerManagementPolicyHandler::
    ~LoginScreenPowerManagementPolicyHandler() {}

void LoginScreenPowerManagementPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {}

DeprecatedIdleActionHandler::DeprecatedIdleActionHandler()
    : IntRangePolicyHandlerBase(
          key::kIdleAction,
          chromeos::PowerPolicyController::ACTION_SUSPEND,
          chromeos::PowerPolicyController::ACTION_DO_NOTHING,
          false) {}

DeprecatedIdleActionHandler::~DeprecatedIdleActionHandler() {}

void DeprecatedIdleActionHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                      PrefValueMap* prefs) {
  // It is safe to use `GetValueUnsafe()` because type checking is performed
  // before the value is used.
  const base::Value* value = policies.GetValueUnsafe(policy_name());
  if (value && EnsureInRange(value, nullptr, nullptr)) {
    if (!prefs->GetValue(ash::prefs::kPowerAcIdleAction, nullptr)) {
      prefs->SetValue(ash::prefs::kPowerAcIdleAction, value->Clone());
    }
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
          SCHEMA_ALLOW_UNKNOWN) {}

PowerManagementIdleSettingsPolicyHandler::
    ~PowerManagementIdleSettingsPolicyHandler() {}

void PowerManagementIdleSettingsPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  std::unique_ptr<base::Value> policy_value;
  if (!CheckAndGetValue(policies, nullptr, &policy_value) || !policy_value) {
    return;
  }
  const base::Value::Dict& policy_value_dict = policy_value->GetDict();

  SetPrefValueIfNotNull(prefs, ash::prefs::kPowerAcScreenDimDelayMs,
                        policy_value_dict.FindByDottedPath(kScreenDimDelayAC));
  SetPrefValueIfNotNull(prefs, ash::prefs::kPowerAcScreenOffDelayMs,
                        policy_value_dict.FindByDottedPath(kScreenOffDelayAC));
  SetPrefValueIfNotNull(
      prefs, ash::prefs::kPowerAcIdleWarningDelayMs,
      policy_value_dict.FindByDottedPath(kIdleWarningDelayAC));

  const base::Value* idle_delay_ac_value =
      policy_value_dict.FindByDottedPath(kIdleDelayAC);
  SetPrefValueIfNotNull(prefs, ash::prefs::kPowerAcIdleDelayMs,
                        idle_delay_ac_value);

  base::Value idle_action_ac_value = CalculateIdleActionValue(
      policy_value_dict.FindByDottedPath(kIdleActionAC), idle_delay_ac_value);
  if (!idle_action_ac_value.is_none()) {
    prefs->SetValue(ash::prefs::kPowerAcIdleAction,
                    std::move(idle_action_ac_value));
  }

  SetPrefValueIfNotNull(
      prefs, ash::prefs::kPowerBatteryScreenDimDelayMs,
      policy_value_dict.FindByDottedPath(kScreenDimDelayBattery));
  SetPrefValueIfNotNull(
      prefs, ash::prefs::kPowerBatteryScreenOffDelayMs,
      policy_value_dict.FindByDottedPath(kScreenOffDelayBattery));
  SetPrefValueIfNotNull(
      prefs, ash::prefs::kPowerBatteryIdleWarningDelayMs,
      policy_value_dict.FindByDottedPath(kIdleWarningDelayBattery));

  const base::Value* idle_delay_battery_value =
      policy_value_dict.FindByDottedPath(kIdleDelayBattery);
  SetPrefValueIfNotNull(prefs, ash::prefs::kPowerBatteryIdleDelayMs,
                        idle_delay_battery_value);

  base::Value idle_action_battery_value = CalculateIdleActionValue(
      policy_value_dict.FindByDottedPath(kIdleActionBattery),
      idle_delay_battery_value);
  if (!idle_action_battery_value.is_none()) {
    prefs->SetValue(ash::prefs::kPowerBatteryIdleAction,
                    std::move(idle_action_battery_value));
  }
}

ScreenLockDelayPolicyHandler::ScreenLockDelayPolicyHandler(
    const Schema& chrome_schema)
    : SchemaValidatingPolicyHandler(
          key::kScreenLockDelays,
          chrome_schema.GetKnownProperty(key::kScreenLockDelays),
          SCHEMA_ALLOW_UNKNOWN) {}

ScreenLockDelayPolicyHandler::~ScreenLockDelayPolicyHandler() {}

void ScreenLockDelayPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  std::unique_ptr<base::Value> policy_value;
  if (!CheckAndGetValue(policies, nullptr, &policy_value) || !policy_value) {
    return;
  }
  const base::Value::Dict& policy_value_dict = policy_value->GetDict();

  SetPrefValueIfNotNull(prefs, ash::prefs::kPowerAcScreenLockDelayMs,
                        policy_value_dict.Find(kScreenLockDelayAC));
  SetPrefValueIfNotNull(prefs, ash::prefs::kPowerBatteryScreenLockDelayMs,
                        policy_value_dict.Find(kScreenLockDelayBattery));
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
  if (!CheckAndGetValue(policies, nullptr, &policy_value) || !policy_value) {
    return;
  }
  const base::Value::Dict& policy_value_dict = policy_value->GetDict();

  SetPrefValueIfNotNull(prefs, ash::prefs::kPowerAcScreenBrightnessPercent,
                        policy_value_dict.Find(kScreenBrightnessPercentAC));
  SetPrefValueIfNotNull(
      prefs, ash::prefs::kPowerBatteryScreenBrightnessPercent,
      policy_value_dict.Find(kScreenBrightnessPercentBattery));
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
  const base::Value* const value =
      policies.GetValue(policy_name(), base::Value::Type::INTEGER);
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

ArcLocationServicePolicyHandler::ArcLocationServicePolicyHandler(
    const char* policy,
    const char* pref)
    : ArcServicePolicyHandler(policy, pref) {}

void ArcLocationServicePolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  // After the Privacy Hub rollout, the Android location toggle will be replaced
  // by the ChromeOS location toggle in the OOBE dialog. This new toggle will be
  // controlled by `kGoogleLocationServicesEnabled`. This new toggle will no
  // longer support force-setting only the initial value during the setup flow,
  // so we can ignore this policy.
  if (ash::features::IsCrosPrivacyHubLocationEnabled()) {
    return;
  }

  // Legacy handling.
  ArcServicePolicyHandler::ApplyPolicySettings(policies, prefs);
}

HelpMeWritePolicyHandler::HelpMeWritePolicyHandler()
    : IntRangePolicyHandlerBase(
          /*policy_name=*/key::kHelpMeWriteSettings,
          /*min=*/
          static_cast<int>(
              HelpMeWritePolicyValue::kEnabledWithModelImprovement),
          /*max=*/static_cast<int>(HelpMeWritePolicyValue::kDisabled),
          /*clamp=*/false) {}

void HelpMeWritePolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                   PrefValueMap* prefs) {
  // It is safe to use `GetValueUnsafe()` because type checking is performed
  // before the value is used.
  const base::Value* value = policies.GetValueUnsafe(policy_name());
  int value_in_range;

  if (value && EnsureInRange(value, &value_in_range, nullptr)) {
    switch (value_in_range) {
      case static_cast<int>(HelpMeWritePolicyValue::kDisabled):
        prefs->SetBoolean(ash::prefs::kOrcaEnabled, false);
        prefs->SetBoolean(ash::prefs::kOrcaFeedbackEnabled, false);
        break;
      case static_cast<int>(
          HelpMeWritePolicyValue::kEnabledWithModelImprovement):
        prefs->SetBoolean(ash::prefs::kOrcaEnabled, true);
        prefs->SetBoolean(ash::prefs::kOrcaFeedbackEnabled, true);
        break;
      case static_cast<int>(
          HelpMeWritePolicyValue::kEnabledWithoutModelImprovement):
        prefs->SetBoolean(ash::prefs::kOrcaEnabled, true);
        prefs->SetBoolean(ash::prefs::kOrcaFeedbackEnabled, false);
        break;
      default:
        LOG(ERROR) << "Policy value out of range.";
        break;
    }
  }
}

}  // namespace policy
