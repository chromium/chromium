// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_conversions.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/json/json_writer.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/schema_registry_service.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_details.h"
#include "components/policy/core/common/policy_merger.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/core/common/schema_map.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/strings/grit/components_strings.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/policy/active_directory_policy_manager.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_store_chromeos.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/device_local_account_policy_service.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "components/user_manager/user_manager.h"
#endif

using base::Value;

namespace em = enterprise_management;

namespace policy {
namespace {

PolicyService* GetPolicyService(Profile* profile) {
  return profile->GetProfilePolicyConnector()->policy_service();
}

// Returns the Schema for |policy_name| if that policy is known. If the policy
// is unknown, returns |base::nullopt|.
base::Optional<Schema> GetKnownPolicySchema(
    const base::Optional<PolicyConversions::PolicyToSchemaMap>&
        known_policy_schemas,
    const std::string& policy_name) {
  if (!known_policy_schemas.has_value())
    return base::nullopt;
  auto known_policy_iterator = known_policy_schemas->find(policy_name);
  if (known_policy_iterator == known_policy_schemas->end())
    return base::nullopt;
  return known_policy_iterator->second;
}

base::Optional<PolicyConversions::PolicyToSchemaMap> GetKnownPolicies(
    const scoped_refptr<SchemaMap> schema_map,
    const PolicyNamespace& policy_namespace) {
  const Schema* schema = schema_map->GetSchema(policy_namespace);
  // There is no policy name verification without valid schema.
  if (!schema || !schema->valid())
    return base::nullopt;

  // Build a vector first and construct the PolicyToSchemaMap (which is a
  // |flat_map|) from that. The reason is that insertion into a |flat_map| is
  // O(n), which would make the loop O(n^2), but constructing from a
  // pre-populated vector is less expensive.
  std::vector<std::pair<std::string, Schema>> policy_to_schema_entries;
  for (auto it = schema->GetPropertiesIterator(); !it.IsAtEnd(); it.Advance()) {
    policy_to_schema_entries.push_back(std::make_pair(it.key(), it.schema()));
  }
  return PolicyConversions::PolicyToSchemaMap(
      std::move(policy_to_schema_entries));
}

}  // namespace

const LocalizedString kPolicySources[POLICY_SOURCE_COUNT] = {
    {"sourceEnterpriseDefault", IDS_POLICY_SOURCE_ENTERPRISE_DEFAULT},
    {"cloud", IDS_POLICY_SOURCE_CLOUD},
    {"sourceActiveDirectory", IDS_POLICY_SOURCE_ACTIVE_DIRECTORY},
    {"sourceDeviceLocalAccountOverride",
     IDS_POLICY_SOURCE_DEVICE_LOCAL_ACCOUNT_OVERRIDE},
    {"platform", IDS_POLICY_SOURCE_PLATFORM},
    {"priorityCloud", IDS_POLICY_SOURCE_CLOUD},
    {"merged", IDS_POLICY_SOURCE_MERGED},
};

PolicyConversions::PolicyConversions() = default;
PolicyConversions::~PolicyConversions() = default;

PolicyConversions& PolicyConversions::WithBrowserContext(
    content::BrowserContext* context) {
  profile_ = Profile::FromBrowserContext(
      chrome::GetBrowserContextRedirectedInIncognito(context));
  return *this;
}

PolicyConversions& PolicyConversions::EnableConvertTypes(bool enabled) {
  convert_types_enabled_ = enabled;
  return *this;
}

PolicyConversions& PolicyConversions::EnableConvertValues(bool enabled) {
  convert_values_enabled_ = enabled;
  return *this;
}

PolicyConversions& PolicyConversions::EnableDeviceLocalAccountPolicies(
    bool enabled) {
  device_local_account_policies_enabled_ = enabled;
  return *this;
}

PolicyConversions& PolicyConversions::EnableDeviceInfo(bool enabled) {
  device_info_enabled_ = enabled;
  return *this;
}

PolicyConversions& PolicyConversions::EnablePrettyPrint(bool enabled) {
  pretty_print_enabled_ = enabled;
  return *this;
}

PolicyConversions& PolicyConversions::EnableUserPolicies(bool enabled) {
  user_policies_enabled_ = enabled;
  return *this;
}

std::string PolicyConversions::ToJSON() {
  return ConvertValueToJSON(ToValue());
}

Value PolicyConversions::GetChromePolicies() {
  PolicyService* policy_service = GetPolicyService(profile_);
  PolicyMap map;

  auto* schema_registry_service = profile_->GetPolicySchemaRegistryService();
  if (!schema_registry_service || !schema_registry_service->registry()) {
    LOG(ERROR) << "Can not dump Chrome policies, no schema registry service";
    return Value(Value::Type::DICTIONARY);
  }

  const scoped_refptr<SchemaMap> schema_map =
      schema_registry_service->registry()->schema_map();

  PolicyNamespace policy_namespace =
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string());

  // Make a copy that can be modified, since some policy values are modified
  // before being displayed.
  map.CopyFrom(policy_service->GetPolicies(policy_namespace));

  // Get a list of all the errors in the policy values.
  const ConfigurationPolicyHandlerList* handler_list =
      g_browser_process->browser_policy_connector()->GetHandlerList();
  PolicyErrorMap errors;
  handler_list->ApplyPolicySettings(map, NULL, &errors);

  // Convert dictionary values to strings for display.
  handler_list->PrepareForDisplaying(&map);

  return GetPolicyValues(map, &errors,
                         GetKnownPolicies(schema_map, policy_namespace));
}

Value PolicyConversions::GetExtensionsPolicies() {
  Value policies(Value::Type::LIST);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Add extension policy values.
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile_);
  if (!registry) {
    LOG(ERROR) << "Can not dump extension policies, no extension registry";
    return policies;
  }
  auto* schema_registry_service = profile_->GetPolicySchemaRegistryService();
  if (!schema_registry_service || !schema_registry_service->registry()) {
    LOG(ERROR) << "Can not dump extension policies, no schema registry service";
    return policies;
  }
  const scoped_refptr<SchemaMap> schema_map =
      schema_registry_service->registry()->schema_map();
  for (const scoped_refptr<const extensions::Extension>& extension :
       registry->enabled_extensions()) {
    // Skip this extension if it's not an enterprise extension.
    if (!extension->manifest()->HasPath(
            extensions::manifest_keys::kStorageManagedSchema)) {
      continue;
    }

    PolicyNamespace policy_namespace =
        PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, extension->id());
    PolicyErrorMap empty_error_map;
    Value extension_policies = GetPolicyValues(
        GetPolicyService(profile_)->GetPolicies(policy_namespace),
        &empty_error_map, GetKnownPolicies(schema_map, policy_namespace));
    Value extension_policies_data(Value::Type::DICTIONARY);
    extension_policies_data.SetKey("name", Value(extension->name()));
    extension_policies_data.SetKey("id", Value(extension->id()));
    extension_policies_data.SetKey("policies", std::move(extension_policies));
    policies.Append(std::move(extension_policies_data));
  }
#endif
  return policies;
}

#if defined(OS_CHROMEOS)
Value PolicyConversions::GetDeviceLocalAccountPolicies() {
  Value policies(Value::Type::LIST);
  // DeviceLocalAccount policies are only available for affiliated users and for
  // system logs.
  if (!device_local_account_policies_enabled_ &&
      (!user_manager::UserManager::IsInitialized() ||
       !user_manager::UserManager::Get()->GetPrimaryUser() ||
       !user_manager::UserManager::Get()->GetPrimaryUser()->IsAffiliated())) {
    return policies;
  }

  // Always includes user policies for device local account policies.
  bool current_use_policy_setup = user_policies_enabled_;
  user_policies_enabled_ = true;

  BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  DCHECK(connector);  // always not-null

  auto* device_local_account_policy_service =
      connector->GetDeviceLocalAccountPolicyService();
  DCHECK(device_local_account_policy_service);  // always non null for
                                                // affiliated users
  std::vector<DeviceLocalAccount> device_local_accounts =
      GetDeviceLocalAccounts(chromeos::CrosSettings::Get());
  for (const auto& account : device_local_accounts) {
    std::string user_id = account.user_id;

    auto* device_local_account_policy_broker =
        device_local_account_policy_service->GetBrokerForUser(user_id);
    if (!device_local_account_policy_broker) {
      LOG(ERROR)
          << "Can not get policy broker for device local account with user id: "
          << user_id;
      continue;
    }

    auto* cloud_policy_core = device_local_account_policy_broker->core();
    DCHECK(cloud_policy_core);
    auto* cloud_policy_store = cloud_policy_core->store();
    DCHECK(cloud_policy_store);

    const scoped_refptr<SchemaMap> schema_map =
        device_local_account_policy_broker->schema_registry()->schema_map();

    PolicyNamespace policy_namespace =
        PolicyNamespace(POLICY_DOMAIN_CHROME, std::string());

    // Make a copy that can be modified, since some policy values are modified
    // before being displayed.
    PolicyMap map;
    map.CopyFrom(cloud_policy_store->policy_map());

    // Get a list of all the errors in the policy values.
    const ConfigurationPolicyHandlerList* handler_list =
        connector->GetHandlerList();
    PolicyErrorMap errors;
    handler_list->ApplyPolicySettings(map, NULL, &errors);

    // Convert dictionary values to strings for display.
    handler_list->PrepareForDisplaying(&map);

    Value current_account_policies = GetPolicyValues(
        map, &errors, GetKnownPolicies(schema_map, policy_namespace));
    Value current_account_policies_data(Value::Type::DICTIONARY);
    current_account_policies_data.SetKey("id", Value(user_id));
    current_account_policies_data.SetKey("user_id", Value(user_id));
    current_account_policies_data.SetKey("name", Value(user_id));
    current_account_policies_data.SetKey("policies",
                                         std::move(current_account_policies));
    policies.Append(std::move(current_account_policies_data));
  }

  // Reset |user_policies_enabled_| setup.
  user_policies_enabled_ = current_use_policy_setup;

  return policies;
}

Value PolicyConversions::GetIdentityFields() {
  Value identity_fields(Value::Type::DICTIONARY);
  if (!device_info_enabled_)
    return Value();
  BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (!connector) {
    LOG(ERROR) << "Can not dump identity fields, no policy connector";
    return Value();
  }
  if (connector->IsEnterpriseManaged()) {
    identity_fields.SetKey("enrollment_domain",
                           Value(connector->GetEnterpriseEnrollmentDomain()));

    if (connector->IsActiveDirectoryManaged()) {
      Value active_directory_info = GetIdentityFieldsFromPolicy(
          connector->GetDeviceActiveDirectoryPolicyManager()
              ->store()
              ->policy());
      identity_fields.MergeDictionary(&active_directory_info);
    }

    if (connector->IsCloudManaged()) {
      Value cloud_info = GetIdentityFieldsFromPolicy(
          connector->GetDeviceCloudPolicyManager()->device_store()->policy());
      identity_fields.MergeDictionary(&cloud_info);
    }
  }
  return identity_fields;
}
#endif

std::string PolicyConversions::ConvertValueToJSON(const Value& value) {
  std::string json_string;
  base::JSONWriter::WriteWithOptions(
      value,
      (pretty_print_enabled_ ? base::JSONWriter::OPTIONS_PRETTY_PRINT : 0),
      &json_string);
  return json_string;
}

Value PolicyConversions::CopyAndMaybeConvert(
    const Value& value,
    const base::Optional<Schema>& schema) {
  Value value_copy = value.Clone();
  if (schema.has_value())
    schema->MaskSensitiveValues(&value_copy);
  if (!convert_values_enabled_)
    return value_copy;
  if (value_copy.is_dict())
    return Value(ConvertValueToJSON(value_copy));

  if (!value_copy.is_list()) {
    return value_copy;
  }

  Value result(Value::Type::LIST);
  for (const auto& element : value_copy.GetList()) {
    if (element.is_dict()) {
      result.Append(Value(ConvertValueToJSON(element)));
    } else {
      result.Append(element.Clone());
    }
  }
  return result;
}

Value PolicyConversions::GetPolicyValue(
    const std::string& policy_name,
    const PolicyMap::Entry& policy,
    PolicyErrorMap* errors,
    const base::Optional<PolicyToSchemaMap>& known_policy_schemas) {
  base::Optional<Schema> known_policy_schema =
      GetKnownPolicySchema(known_policy_schemas, policy_name);
  Value value(Value::Type::DICTIONARY);
  value.SetKey("value",
               CopyAndMaybeConvert(*policy.value, known_policy_schema));
  if (convert_types_enabled_) {
    value.SetKey(
        "scope",
        Value((policy.scope == POLICY_SCOPE_USER) ? "user" : "machine"));
    value.SetKey("level", Value(Value((policy.level == POLICY_LEVEL_RECOMMENDED)
                                          ? "recommended"
                                          : "mandatory")));
    value.SetKey("source", Value(kPolicySources[policy.source].name));
  } else {
    value.SetKey("scope", Value(policy.scope));
    value.SetKey("level", Value(policy.level));
    value.SetKey("source", Value(policy.source));
  }

  // Policies that have at least one source that could not be merged will
  // still be treated as conflicted policies while policies that had all of
  // their sources merged will not be considered conflicted anymore. Some
  // policies have only one source but still appear as POLICY_SOURCE_MERGED
  // because all policies that are listed as policies that should be merged are
  // treated as merged regardless the number of sources. Those policies will not
  // be treated as conflicted policies.
  if (policy.source == POLICY_SOURCE_MERGED) {
    bool policy_has_unmerged_source = false;
    for (const auto& conflict : policy.conflicts) {
      if (PolicyMerger::ConflictCanBeMerged(conflict, policy))
        continue;
      policy_has_unmerged_source = true;
      break;
    }
    value.SetKey("allSourcesMerged", Value(policy.conflicts.size() <= 1 ||
                                           !policy_has_unmerged_source));
  }

  base::string16 error;
  if (!known_policy_schema.has_value()) {
    // We don't know what this policy is. This is an important error to
    // show.
    error = l10n_util::GetStringUTF16(IDS_POLICY_UNKNOWN);
  } else {
    // The PolicyMap contains errors about retrieving the policy, while the
    // PolicyErrorMap contains validation errors. Concat the errors.
    auto policy_map_errors = policy.GetLocalizedErrors(
        base::BindRepeating(&l10n_util::GetStringUTF16));
    auto error_map_errors = errors->GetErrors(policy_name);
    if (policy_map_errors.empty())
      error = error_map_errors;
    else if (error_map_errors.empty())
      error = policy_map_errors;
    else
      error =
          base::JoinString({policy_map_errors, errors->GetErrors(policy_name)},
                           base::ASCIIToUTF16("\n"));
  }
  if (!error.empty())
    value.SetKey("error", Value(error));

  base::string16 warning = policy.GetLocalizedWarnings(
      base::BindRepeating(&l10n_util::GetStringUTF16));
  if (!warning.empty())
    value.SetKey("warning", Value(warning));

  if (policy.IsBlockedOrIgnored())
    value.SetBoolKey("ignored", true);

  if (!policy.conflicts.empty()) {
    Value conflict_values(Value::Type::LIST);
    for (const auto& conflict : policy.conflicts) {
      base::Value conflicted_policy_value =
          GetPolicyValue(policy_name, conflict, errors, known_policy_schemas);
      conflict_values.Append(std::move(conflicted_policy_value));
    }

    value.SetKey("conflicts", std::move(conflict_values));
  }

  return value;
}

Value PolicyConversions::GetPolicyValues(
    const PolicyMap& map,
    PolicyErrorMap* errors,
    const base::Optional<PolicyToSchemaMap>& known_policy_schemas) {
  base::Value values(base::Value::Type::DICTIONARY);
  for (const auto& entry : map) {
    const std::string& policy_name = entry.first;
    const PolicyMap::Entry& policy = entry.second;
    if (policy.scope == POLICY_SCOPE_USER && !user_policies_enabled_)
      continue;
    base::Value value =
        GetPolicyValue(policy_name, policy, errors, known_policy_schemas);
    values.SetKey(policy_name, std::move(value));
  }
  return values;
}

#if defined(OS_CHROMEOS)
Value PolicyConversions::GetIdentityFieldsFromPolicy(
    const em::PolicyData* policy) {
  Value identity_fields(Value::Type::DICTIONARY);
  if (!policy) {
    return identity_fields;
  }

  if (policy->has_device_id())
    identity_fields.SetKey("client_id", Value(policy->device_id()));

  if (policy->has_annotated_location()) {
    identity_fields.SetKey("device_location",
                           Value(policy->annotated_location()));
  }

  if (policy->has_annotated_asset_id())
    identity_fields.SetKey("asset_id", Value(policy->annotated_asset_id()));

  if (policy->has_display_domain())
    identity_fields.SetKey("display_domain", Value(policy->display_domain()));

  if (policy->has_machine_name())
    identity_fields.SetKey("machine_name", Value(policy->machine_name()));

  return identity_fields;
}

#endif  // defined(OS_CHROMEOS)

/**
 * DictionaryPolicyConversions
 */

DictionaryPolicyConversions::DictionaryPolicyConversions() = default;
DictionaryPolicyConversions::~DictionaryPolicyConversions() = default;

Value DictionaryPolicyConversions::ToValue() {
  Value all_policies(Value::Type::DICTIONARY);

  if (profile()) {
    all_policies.SetKey("chromePolicies", GetChromePolicies());

#if BUILDFLAG(ENABLE_EXTENSIONS)
    all_policies.SetKey("extensionPolicies", GetExtensionsPolicies());
#endif
  }

#if defined(OS_CHROMEOS)
  all_policies.SetKey("deviceLocalAccountPolicies",
                      GetDeviceLocalAccountPolicies());
  Value identity_fields = GetIdentityFields();
  if (!identity_fields.is_none())
    all_policies.MergeDictionary(&identity_fields);
#endif  // defined(OS_CHROMEOS)
  return all_policies;
}

#if defined(OS_CHROMEOS)
Value DictionaryPolicyConversions::GetDeviceLocalAccountPolicies() {
  Value policies = PolicyConversions::GetDeviceLocalAccountPolicies();
  Value device_values(Value::Type::DICTIONARY);
  for (auto&& policy : policies.GetList()) {
    device_values.SetKey(policy.FindKey("id")->GetString(),
                         std::move(*policy.FindKey("policies")));
  }
  return device_values;
}
#endif

Value DictionaryPolicyConversions::GetExtensionsPolicies() {
  Value policies = PolicyConversions::GetExtensionsPolicies();
  Value extension_values(Value::Type::DICTIONARY);
  for (auto&& policy : policies.GetList()) {
    extension_values.SetKey(policy.FindKey("id")->GetString(),
                            std::move(*policy.FindKey("policies")));
  }
  return extension_values;
}

/**
 * ArrayPolicyConversions
 */

ArrayPolicyConversions::ArrayPolicyConversions() = default;
ArrayPolicyConversions::~ArrayPolicyConversions() = default;

Value ArrayPolicyConversions::ToValue() {
  Value all_policies(Value::Type::LIST);

  if (profile()) {
    all_policies.Append(GetChromePolicies());

#if BUILDFLAG(ENABLE_EXTENSIONS)
    Value extension_policies = GetExtensionsPolicies();
    all_policies.GetList().insert(
        all_policies.GetList().end(),
        std::make_move_iterator(extension_policies.GetList().begin()),
        std::make_move_iterator(extension_policies.GetList().end()));
#endif
  }

#if defined(OS_CHROMEOS)
  Value device_policeis = GetDeviceLocalAccountPolicies();
  all_policies.GetList().insert(
      all_policies.GetList().end(),
      std::make_move_iterator(device_policeis.GetList().begin()),
      std::make_move_iterator(device_policeis.GetList().end()));

  Value identity_fields = GetIdentityFields();
  if (!identity_fields.is_none())
    all_policies.Append(std::move(identity_fields));
#endif  // defined(OS_CHROMEOS)

  return all_policies;
}

Value ArrayPolicyConversions::GetChromePolicies() {
  Value chrome_policies_data(Value::Type::DICTIONARY);
  chrome_policies_data.SetKey("name", Value("Chrome Policies"));
  chrome_policies_data.SetKey("policies",
                              PolicyConversions::GetChromePolicies());
  return chrome_policies_data;
}

}  // namespace policy
