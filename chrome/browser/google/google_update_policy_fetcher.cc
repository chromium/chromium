// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_update_policy_fetcher.h"

#include <optional>
#include <utility>

#include "base/json/json_reader.h"
#include "base/json/values_util.h"
#include "base/memory/raw_ref.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema.h"

base::Value GetGoogleUpdatePolicyNames() {
  base::ListValue names;
  for (const auto& key_value : GetGoogleUpdatePolicySchemas()) {
    names.Append(base::Value(key_value.first));
  }
  return base::Value(std::move(names));
}

policy::PolicyConversions::PolicyToSchemaMap GetGoogleUpdatePolicySchemas() {
  // TODO(crbug.com/40722467): Use actual schemas.
  return policy::PolicyConversions::PolicyToSchemaMap{{
      {kAutoUpdateCheckPeriodMinutes, policy::Schema()},
      {kDownloadPreference, policy::Schema()},
      {kForceInstallApps, policy::Schema()},
      {kInstallPolicy, policy::Schema()},
      {kProxyMode, policy::Schema()},
      {kProxyPacUrl, policy::Schema()},
      {kProxyServer, policy::Schema()},
      {kRollbackToTargetVersion, policy::Schema()},
      {kTargetVersionPrefix, policy::Schema()},
      {kTargetChannel, policy::Schema()},
      {kUpdatePolicy, policy::Schema()},
      {kUpdatesSuppressedDurationMin, policy::Schema()},
      {kUpdatesSuppressedStartHour, policy::Schema()},
      {kUpdatesSuppressedStartMinute, policy::Schema()},
      {kCloudPolicyOverridesPlatformPolicy, policy::Schema()},
      {kMajorVersionRolloutPolicy, policy::Schema()},
      {kMinorVersionRolloutPolicy, policy::Schema()},
  }};
}

namespace {

// Maps a policy source string from the updater's JSON format to the
// PolicySource enum. Returns std::nullopt if the source is unknown.
std::optional<policy::PolicySource> PolicySourceFromString(
    std::string_view source) {
  if (source == "Device Management") {
    return policy::POLICY_SOURCE_CLOUD;
  }
  if (source == "Managed Preferences" || source == "Group Policy") {
    return policy::POLICY_SOURCE_PLATFORM;
  }
  if (source == "Default") {
    return policy::POLICY_SOURCE_ENTERPRISE_DEFAULT;
  }
  return std::nullopt;
}

struct PrevailingSourceAndValue {
  policy::PolicySource source;
  raw_ref<const base::Value> value;
};

// Returns the prevailing source and value for `json_key` in `source_dict`, or
// std::nullopt if the policy, source, or prevailing value is missing/unknown.
std::optional<PrevailingSourceAndValue> GetPrevailingPolicySourceAndValue(
    const base::DictValue& source_dict,
    std::string_view json_key) {
  const base::DictValue* policy_status = source_dict.FindDict(json_key);
  if (!policy_status) {
    return std::nullopt;
  }
  const std::string* prevailing_source =
      policy_status->FindString("prevailingSource");
  const base::DictValue* values_by_source =
      policy_status->FindDict("valuesBySource");
  if (!prevailing_source || !values_by_source) {
    return std::nullopt;
  }
  const base::Value* prevailing_value =
      values_by_source->Find(*prevailing_source);
  std::optional<policy::PolicySource> prevailing_source_enum =
      PolicySourceFromString(*prevailing_source);
  if (!prevailing_value || !prevailing_source_enum) {
    return std::nullopt;
  }
  return PrevailingSourceAndValue{*prevailing_source_enum,
                                  raw_ref(*prevailing_value)};
}

// Parses a single policy entry from the updater's "Policy Set" JSON format
// (with "prevailingSource" and "valuesBySource") and adds it to `policies`.
void MapPolicyFromJson(std::string_view legacy_key,
                       std::string_view json_key,
                       const base::DictValue& source_dict,
                       policy::PolicyMap* policies) {
  std::optional<PrevailingSourceAndValue> prevailing_source_and_value =
      GetPrevailingPolicySourceAndValue(source_dict, json_key);
  if (!prevailing_source_and_value) {
    return;
  }
  policies->Set(std::string(legacy_key), policy::POLICY_LEVEL_MANDATORY,
                policy::POLICY_SCOPE_MACHINE,
                prevailing_source_and_value->source,
                prevailing_source_and_value->value->Clone(),
                /* external_data_fetcher= */ nullptr);
}

// Maps the "UpdatesSuppressed" composite policy from the updater's JSON format.
void MapUpdatesSuppressedPolicy(const base::DictValue& source_dict,
                                policy::PolicyMap* policies) {
  std::optional<PrevailingSourceAndValue> prevailing_source_and_value =
      GetPrevailingPolicySourceAndValue(source_dict, "UpdatesSuppressed");
  if (!prevailing_source_and_value) {
    return;
  }
  const base::DictValue* suppressed_val =
      prevailing_source_and_value->value->GetIfDict();
  if (!suppressed_val) {
    return;
  }

  auto add_suppressed_part = [&](std::string_view legacy_key,
                                 std::string_view json_sub_key) {
    std::optional<int> part_val = suppressed_val->FindInt(json_sub_key);
    if (part_val) {
      policies->Set(std::string(legacy_key), policy::POLICY_LEVEL_MANDATORY,
                    policy::POLICY_SCOPE_MACHINE,
                    prevailing_source_and_value->source, base::Value(*part_val),
                    /* external_data_fetcher= */ nullptr);
    }
  };
  add_suppressed_part(kUpdatesSuppressedStartHour, "StartHour");
  add_suppressed_part(kUpdatesSuppressedStartMinute, "StartMinute");
  add_suppressed_part(kUpdatesSuppressedDurationMin, "Duration");
}

// Maps the "LastCheckPeriod" policy from the updater's JSON format (serialized
// as microseconds by base::TimeDeltaToValue) to AutoUpdateCheckPeriodMinutes
// (in minutes).
void MapLastCheckPeriodPolicy(const base::DictValue& source_dict,
                              policy::PolicyMap* policies) {
  std::optional<PrevailingSourceAndValue> prevailing_source_and_value =
      GetPrevailingPolicySourceAndValue(source_dict, "LastCheckPeriod");
  if (!prevailing_source_and_value) {
    return;
  }

  std::optional<base::TimeDelta> period =
      base::ValueToTimeDelta(*prevailing_source_and_value->value);
  if (!period) {
    return;
  }

  policies->Set(std::string(kAutoUpdateCheckPeriodMinutes),
                policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                prevailing_source_and_value->source,
                base::Value(period->InMinutes()),
                /* external_data_fetcher= */ nullptr);
}

}  // namespace

// Parses the policies JSON from the updater and populates the given PolicyMap.
// The JSON follows the "Policy Set" format defined in
// docs/updater/history_log.md and aligns with the format used by the
// chrome://updater page.
void ParsePoliciesJsonIntoPolicyMap(std::string_view json,
                                    std::string_view app_id,
                                    policy::PolicyMap* policies) {
  if (json.empty()) {
    return;
  }
  std::optional<base::Value> value =
      base::JSONReader::Read(json, base::JSON_PARSE_RFC);
  if (!value || !value->is_dict()) {
    return;
  }
  const base::DictValue* dict_ptr = value->GetIfDict();
  if (!dict_ptr) {
    return;
  }
  const base::DictValue& dict = *dict_ptr;

  // Parse global (non-app-specific) policies from "policiesByName".
  const base::DictValue* policies_by_name = dict.FindDict("policiesByName");
  if (policies_by_name) {
    MapLastCheckPeriodPolicy(*policies_by_name, policies);
    MapPolicyFromJson(kDownloadPreference, "DownloadPreference",
                      *policies_by_name, policies);
    MapPolicyFromJson(kProxyMode, "ProxyMode", *policies_by_name, policies);
    MapPolicyFromJson(kProxyPacUrl, "ProxyPacURL", *policies_by_name, policies);
    MapPolicyFromJson(kProxyServer, "ProxyServer", *policies_by_name, policies);
    MapPolicyFromJson(kForceInstallApps, "ForceInstallApps", *policies_by_name,
                      policies);
    MapPolicyFromJson(kCloudPolicyOverridesPlatformPolicy,
                      "CloudPolicyOverridesPlatformPolicy", *policies_by_name,
                      policies);

    MapUpdatesSuppressedPolicy(*policies_by_name, policies);
  }

  // Parse per-app policies from "policiesByAppId".
  const base::DictValue* policies_by_app_id = dict.FindDict("policiesByAppId");

  if (policies_by_app_id == nullptr) {
    return;
  }

  const base::DictValue* app_policies = nullptr;
  for (const auto [key, val] : *policies_by_app_id) {
    if (base::EqualsCaseInsensitiveASCII(key, app_id) && val.is_dict()) {
      app_policies = &val.GetDict();
      break;
    }
  }
  if (app_policies == nullptr) {
    return;
  }

  MapPolicyFromJson(kUpdatePolicy, "Update", *app_policies, policies);
  MapPolicyFromJson(kInstallPolicy, "Install", *app_policies, policies);
  MapPolicyFromJson(kTargetVersionPrefix, "TargetVersionPrefix", *app_policies,
                    policies);
  MapPolicyFromJson(kTargetChannel, "TargetChannel", *app_policies, policies);
  MapPolicyFromJson(kRollbackToTargetVersion, "RollbackToTargetVersionAllowed",
                    *app_policies, policies);
  MapPolicyFromJson(kMajorVersionRolloutPolicy, "MajorVersionRolloutPolicy",
                    *app_policies, policies);
  MapPolicyFromJson(kMinorVersionRolloutPolicy, "MinorVersionRolloutPolicy",
                    *app_policies, policies);
}
