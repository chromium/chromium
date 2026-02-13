// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/value_provider/extension_install_policies_value_provider.h"

#include "base/feature_list.h"
#include "chrome/browser/policy/cloud/extension_install_policy_service.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/value_provider/value_provider_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/browser/policy_conversions.h"
#include "components/policy/core/common/features.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/pref_names.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"

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
  if (!base::FeatureList::IsEnabled(
          policy::features::kEnableExtensionInstallPolicyFetching)) {
    return base::DictValue();
  }
  if (!profile_->GetPrefs()->GetBoolean(
          extensions::pref_names::kExtensionInstallCloudPolicyChecksEnabled)) {
    return base::DictValue();
  }
  auto* policy_service = GetPolicyService(&profile_.get());
  if (!policy_service) {
    return base::DictValue();
  }
  const extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(&profile_.get());
  CHECK(registry);

  // Create a `dict` value like this:
  // {
  //   "Extension Name (aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa@1.2.3)": {
  //     "level": "mandatory",
  //     "scope": "machine",
  //     "source": "cloud",
  //     "value": {
  //       "value": "blocked",
  //       "reasons": ["category", "risk_score"]
  //     }
  //   },
  //   ...
  // }
  base::DictValue dict;
  const policy::PolicyMap& extension_install_policy_map =
      policy_service->GetPolicies(policy::PolicyNamespace(
          policy::POLICY_DOMAIN_EXTENSION_INSTALL, std::string()));
  for (const auto& [extension_id, entry] : extension_install_policy_map) {
    const base::Value* policy_value = entry.value(base::Value::Type::DICT);
    if (!policy_value) {
      continue;
    }

    for (const auto [extension_version, value] : policy_value->GetDict()) {
      em::ExtensionInstallPolicy::Action action =
          static_cast<em::ExtensionInstallPolicy::Action>(
              value.GetDict().FindInt("action").value_or(
                  em::ExtensionInstallPolicy::ACTION_ALLOW));
      base::DictValue policy_value_dict =
          base::DictValue()
              .Set("action", ActionToString(action))
              .Set("reasons",
                   ReasonsToListValue(value.GetDict().FindList("reasons")));
      if (auto* risk_levels = value.GetDict().FindDict("risk_levels")) {
        policy_value_dict.Set("risk_levels", RiskLevelsToValue(risk_levels));
      }
      // TODO(nicolaso): Show actual extension names instead of IDs.
      base::DictValue policy_dict =
          base::DictValue()
              .Set("value", std::move(policy_value_dict))
              .Set("level", LevelToString(entry.level))
              .Set("scope", ScopeToString(entry.scope))
              .Set("source", SourceToString(entry.source));
      if (const auto* extension =
              registry->GetInstalledExtension(extension_id)) {
        dict.Set(absl::StrFormat("%s (%s@%s)", extension->name(), extension_id,
                                 extension_version),
                 std::move(policy_dict));
      } else {
        dict.Set(absl::StrFormat("%s@%s", extension_id, extension_version),
                 std::move(policy_dict));
      }
    }
  }

  return base::DictValue().Set(
      policy::kExtensionInstallPoliciesId,
      base::DictValue()
          .Set(policy::kNameKey, policy::kExtensionInstallPoliciesName)
          .Set(policy::kPoliciesKey, std::move(dict)));
}

base::DictValue ExtensionInstallPoliciesValueProvider::GetNames() {
  if (!base::FeatureList::IsEnabled(
          policy::features::kEnableExtensionInstallPolicyFetching)) {
    return base::DictValue();
  }
  if (!profile_->GetPrefs()->GetBoolean(
          extensions::pref_names::kExtensionInstallCloudPolicyChecksEnabled)) {
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
