// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/value_provider/extension_install_policies_value_provider.h"

#include <algorithm>
#include <optional>
#include <tuple>
#include <vector>

#include "base/feature_list.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/policy/cloud/extension_install_policy_service.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/value_provider/value_provider_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/browser/policy_conversions.h"
#include "components/policy/core/common/features.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/pref_names.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "ui/base/l10n/l10n_util.h"

namespace em = enterprise_management;

namespace {

std::string ActionToString(em::ExtensionInstallPolicy::Action action) {
  switch (action) {
    case em::ExtensionInstallPolicy::ACTION_BLOCK:
      return "block";
    case em::ExtensionInstallPolicy::ACTION_ALLOW:
      return "allow";
    default:
      return "unknown";
  }
}

base::ListValue ReasonsToListValue(const base::ListValue* reasons) {
  if (!reasons) {
    return base::ListValue();
  }
  base::ListValue result;
  for (const auto& reason : *reasons) {
    switch (reason.GetInt()) {
      case em::ExtensionInstallPolicy::REASON_BLOCKED_CATEGORY:
        result.Append("category");
        break;
      case em::ExtensionInstallPolicy::REASON_RISK_SCORE:
        result.Append("risk_score");
        break;
      default:
        result.Append("unknown");
        break;
    }
  }
  return result;
}

base::DictValue RiskLevelsToValue(const base::DictValue* risk_levels) {
  if (!risk_levels) {
    return base::DictValue();
  }
  base::DictValue result;
  for (const auto [provider_name, risk_level] : *risk_levels) {
    switch (risk_level.GetInt()) {
      case em::RiskLevel::RISK_LEVEL_LOW:
        result.Set(provider_name, "low");
        break;
      case em::RiskLevel::RISK_LEVEL_MEDIUM:
        result.Set(provider_name, "medium");
        break;
      case em::RiskLevel::RISK_LEVEL_HIGH:
        result.Set(provider_name, "high");
        break;
      default:
        result.Set(provider_name, "unknown");
        break;
    }
  }
  return result;
}

std::string LevelToString(policy::PolicyLevel level) {
  static_assert(policy::POLICY_LEVEL_MAX == 1);
  switch (level) {
    case policy::POLICY_LEVEL_MANDATORY:
      return "mandatory";
    case policy::POLICY_LEVEL_RECOMMENDED:
      return "recommended";
  }
  NOTREACHED();
}

std::string ScopeToString(policy::PolicyScope scope) {
  static_assert(policy::POLICY_SCOPE_MAX == 1);
  switch (scope) {
    case policy::POLICY_SCOPE_MACHINE:
      return "machine";
    case policy::POLICY_SCOPE_USER:
      return "user";
  }
  NOTREACHED();
}

std::string SourceToString(policy::PolicySource source) {
  switch (source) {
    case policy::POLICY_SOURCE_CLOUD:
      return "cloud";
    default:
      // Any other value shouldn't happen, because these policies always come
      // from the cloud.
      return "unknown";
  }
}

// A helper struct to manage policy entry data, facilitating sorting and
// conversion to the format expected by the policy UI.
struct PolicyEntryData {
  PolicyEntryData(const std::string& version,
                  const policy::PolicyMap::Entry* entry)
      : entry_(raw_ref<const policy::PolicyMap::Entry>::from_ptr(entry)),
        value_(raw_ref<const base::DictValue>::from_ptr(
            entry_->value(base::Value::Type::DICT)
                ->GetDict()
                .FindDict(version))) {
    action_ = static_cast<em::ExtensionInstallPolicy::Action>(
        value_->FindInt("action").value_or(
            em::ExtensionInstallPolicy::ACTION_ALLOW));
  }

  // Determines if this policy entry has higher priority than `other`.
  // "Block" actions always take precedence over others (ACTION_BLOCK = 2,
  // ACTION_ALLOW = 1). If actions are the same, standard level and scope
  // precedence rules apply (Mandatory > Recommended, Machine > User). This is
  // only used for UI presentation purposes; the actual policy precedence logic
  // is handled by the ExtensionInstallPolicyService and there no actual
  // precedence difference between user/machine scope.
  bool operator>(const PolicyEntryData& other) const {
    return std::tie(action_, entry_->level, entry_->scope) >
           std::tie(other.action_, other.entry_->level, other.entry_->scope);
  }

  // Converts the entry's metadata and version-specific value into a dictionary
  // formatted for the policy UI.
  base::DictValue ToDict() const {
    base::DictValue dict;
    {
      base::DictValue val_dict;
      val_dict.Set("action", ActionToString(action_));
      val_dict.Set("reasons", ReasonsToListValue(value_->FindList("reasons")));
      if (const auto* risk_levels = value_->FindDict("risk_levels")) {
        val_dict.Set("risk_levels", RiskLevelsToValue(risk_levels));
      }
      dict.Set("value", std::move(val_dict));
    }
    dict.Set("level", base::Value(LevelToString(entry_->level)));
    dict.Set("scope", base::Value(ScopeToString(entry_->scope)));
    dict.Set("source", base::Value(SourceToString(entry_->source)));
    return dict;
  }

  const base::DictValue& value() const { return value_.get(); }

 private:
  raw_ref<const policy::PolicyMap::Entry> entry_;
  raw_ref<const base::DictValue> value_;
  em::ExtensionInstallPolicy::Action action_;
};

// Processes a policy entry and its conflicts to determine the primary policy to
// display (the one that "wins") and categorizes all others as either conflicts
// (different value) or superseded (same value).
base::DictValue GetAggregatedPolicyValueForExtension(
    const std::string& extension_version,
    const policy::PolicyMap::Entry& entry) {
  std::vector<PolicyEntryData> entries;
  entries.emplace_back(extension_version, &entry);
  for (const auto& conflict : entry.conflicts) {
    entries.emplace_back(extension_version, &conflict.entry());
  }

  std::ranges::sort(entries, std::greater<>());

  base::DictValue dict = entries[0].ToDict();
  base::ListValue conflicts;
  base::ListValue superseded;

  for (size_t i = 1; i < entries.size(); ++i) {
    if (entries[0].value() == entries[i].value()) {
      superseded.Append(entries[i].ToDict());
    } else {
      conflicts.Append(entries[i].ToDict());
    }
  }

  if (!conflicts.empty()) {
    dict.Set("conflicts", std::move(conflicts));
  }
  if (!superseded.empty()) {
    dict.Set("superseded", std::move(superseded));
  }

  return dict;
}

base::DictValue GetPoliciesValuesDict(base::DictValue policies_values_dict) {
  return base::DictValue().Set(
      policy::kExtensionInstallPoliciesId,
      base::DictValue()
          .Set(policy::kNameKey, policy::kExtensionInstallPoliciesName)
          .Set(policy::kPoliciesKey, std::move(policies_values_dict)));
}

}  // namespace

ExtensionInstallPoliciesValueProvider::ExtensionInstallPoliciesValueProvider(
    Profile* profile,
    policy::ExtensionInstallPolicyService* service)
    : profile_(raw_ref<Profile>::from_ptr(profile)) {
  CHECK(service);
  observation_.Observe(service);
}

ExtensionInstallPoliciesValueProvider::
    ~ExtensionInstallPoliciesValueProvider() = default;

base::DictValue ExtensionInstallPoliciesValueProvider::GetValues() {
  base::DictValue policies_values_dict;

  if (!base::FeatureList::IsEnabled(
          policy::features::kEnableExtensionInstallPolicyFetching)) {
    return GetPoliciesValuesDict(std::move(policies_values_dict));
  }

  if (!profile_->GetPrefs()->GetBoolean(
          extensions::pref_names::kExtensionInstallCloudPolicyChecksEnabled)) {
    return GetPoliciesValuesDict(std::move(policies_values_dict));
  }
  auto* policy_service = GetPolicyService(&profile_.get());
  if (!policy_service) {
    return GetPoliciesValuesDict(std::move(policies_values_dict));
  }
  const extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(&profile_.get());
  CHECK(registry);

  auto* extension_management =
      extensions::ExtensionManagementFactory::GetForBrowserContext(
          &profile_.get());
  CHECK(extension_management);

  // Create a `dict` value like this:
  // {
  //   "Extension Name (aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa@1.2.3)": {
  //     "level": "mandatory",
  //     "scope": "machine",
  //     "source": "cloud",
  //     "value": {
  //       "action": "blocked",
  //       "reasons": ["category", "risk_score"]
  //     },
  //     "conflicts": [ ... ],
  //     "superseded": [ ... ]
  //   },
  //   ...
  // }
  const policy::PolicyMap& extension_install_policy_map =
      policy_service->GetPolicies(policy::PolicyNamespace(
          policy::POLICY_DOMAIN_EXTENSION_INSTALL, std::string()));
  for (const auto& [extension_id, entry] : extension_install_policy_map) {
    const base::Value* policy_value = entry.value(base::Value::Type::DICT);
    if (!policy_value) {
      continue;
    }

    bool is_ignored =
        extension_management->IsInstallationExplicitlyAllowed(extension_id) ||
        extension_management->IsInstallationExplicitlyBlocked(extension_id);

    for (const auto [extension_version, value] : policy_value->GetDict()) {
      base::DictValue policy_dict =
          GetAggregatedPolicyValueForExtension(extension_version, entry);

      if (is_ignored) {
        policy_dict.Set("ignored", true);
        policy_dict.Set(
            "info",
            l10n_util::GetStringUTF16(
                IDS_POLICY_EXTENSION_INSTALL_IGNORED_BY_INSTALLATION_MODE));
      }

      if (const auto* extension =
              registry->GetInstalledExtension(extension_id)) {
        policies_values_dict.Set(
            absl::StrFormat("%s (%s@%s)", extension->name(), extension_id,
                            extension_version),
            std::move(policy_dict));
      } else {
        policies_values_dict.Set(
            absl::StrFormat("%s@%s", extension_id, extension_version),
            std::move(policy_dict));
      }
    }
  }

  return GetPoliciesValuesDict(std::move(policies_values_dict));
}

base::DictValue ExtensionInstallPoliciesValueProvider::GetNames() {
  if (!base::FeatureList::IsEnabled(
          policy::features::kEnableExtensionInstallPolicyFetching)) {
    return base::DictValue();
  }
  return base::DictValue().Set(
      policy::kExtensionInstallPoliciesId,
      base::DictValue()
          .Set(policy::kNameKey, policy::kExtensionInstallPoliciesName)
          .Set(policy::kPolicyNamesKey, base::ListValue()));
}

void ExtensionInstallPoliciesValueProvider::OnExtensionInstallPolicyUpdated() {
  NotifyValueChange();
}
