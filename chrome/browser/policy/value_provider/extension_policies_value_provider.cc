// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/value_provider/extension_policies_value_provider.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/policy/chrome_policy_conversions_client.h"
#include "chrome/browser/policy/schema_registry_service.h"
#include "chrome/browser/policy/value_provider/value_provider_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/browser/policy_conversions.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/core/common/schema_map.h"
#include "components/policy/core/common/schema_registry.h"
#include "components/policy/resources/webui/mojom/policy.mojom.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace {

bool ContainsStorageManagedSchema(const extensions::Extension* extension) {
  return extension->manifest()->FindPath(
      extensions::manifest_keys::kStorageManagedSchema);
}

// Looks for policy::kIdKey in `policy` dictionary and adds it to
// `extension_policies` with the ID value as a key. Moves `policy` when adding.
void AddExtensionPolicyValueToDict(base::Value& policy,
                                   base::DictValue& extension_policies) {
  base::DictValue* policy_dict = policy.GetIfDict();
  if (!policy_dict) {
    return;
  }
  std::string* id = policy_dict->FindString(policy::kIdKey);
  if (!id) {
    return;
  }
  policy_dict->Remove(*id);
  extension_policies.Set(*id, std::move(policy));
}

Profile* GetExtensionProfile(Profile* profile,
                             policy::PolicyDomain policy_domain) {
#if BUILDFLAG(IS_CHROMEOS)
  return policy_domain == policy::POLICY_DOMAIN_SIGNIN_EXTENSIONS
             ? ash::ProfileHelper::GetSigninProfile()
             : profile;
#else   // BUILDFLAG(IS_CHROMEOS)
  return profile;
#endif  // BUILDFLAG(IS_CHROMEOS)
}

}  // namespace

ExtensionPoliciesValueProvider::ExtensionPoliciesValueProvider(Profile* profile)
    : profile_(profile) {
  extension_registry_observation_.Observe(
      extensions::ExtensionRegistry::Get(profile_));
  policy_service_observation_.Observe(GetPolicyService(profile_));
}

ExtensionPoliciesValueProvider::~ExtensionPoliciesValueProvider() = default;

base::DictValue ExtensionPoliciesValueProvider::GetValues() {
  base::DictValue extension_policies;
  auto client =
      std::make_unique<policy::ChromePolicyConversionsClient>(profile_);
  if (client->HasUserPolicies()) {
    for (auto& policy :
         client->GetExtensionPolicies(policy::POLICY_DOMAIN_EXTENSIONS)) {
      AddExtensionPolicyValueToDict(policy, extension_policies);
    }
  }
#if BUILDFLAG(IS_CHROMEOS)
  for (auto& policy :
       client->GetExtensionPolicies(policy::POLICY_DOMAIN_SIGNIN_EXTENSIONS)) {
    AddExtensionPolicyValueToDict(policy, extension_policies);
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
  return extension_policies;
}

base::DictValue ExtensionPoliciesValueProvider::GetNames() {
  base::DictValue extension_policy_names =
      GetExtensionPolicyNames(policy::POLICY_DOMAIN_EXTENSIONS);
#if BUILDFLAG(IS_CHROMEOS)
  extension_policy_names.Merge(
      GetExtensionPolicyNames(policy::POLICY_DOMAIN_SIGNIN_EXTENSIONS));
#endif  // BUILDFLAG(IS_CHROMEOS)
  return extension_policy_names;
}

policy::PolicyGroupMojoMap ExtensionPoliciesValueProvider::GetValuesMojo()
    const {
  policy::PolicyGroupMojoMap extension_policies;
  auto client =
      std::make_unique<policy::ChromePolicyConversionsClient>(profile_);
  if (client->HasUserPolicies()) {
    extension_policies =
        client->GetExtensionPoliciesMojo(policy::POLICY_DOMAIN_EXTENSIONS);
  }
#if BUILDFLAG(IS_CHROMEOS)
  for (auto& [id, group] : client->GetExtensionPoliciesMojo(
           policy::POLICY_DOMAIN_SIGNIN_EXTENSIONS)) {
    extension_policies.insert_or_assign(id, std::move(group));
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
  return extension_policies;
}

policy::PolicyGroupNamesMojoMap ExtensionPoliciesValueProvider::GetNamesMojo()
    const {
  policy::PolicyGroupNamesMojoMap extension_policy_names =
      GetExtensionPolicyNamesMojo(policy::POLICY_DOMAIN_EXTENSIONS);
#if BUILDFLAG(IS_CHROMEOS)
  for (auto& [id, group_names] :
       GetExtensionPolicyNamesMojo(policy::POLICY_DOMAIN_SIGNIN_EXTENSIONS)) {
    extension_policy_names.insert_or_assign(id, std::move(group_names));
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
  return extension_policy_names;
}

base::DictValue ExtensionPoliciesValueProvider::GetExtensionPolicyNames(
    policy::PolicyDomain policy_domain) const {
  base::DictValue names;
  Profile* extension_profile =
      GetExtensionProfile(profile_.get(), policy_domain);

  scoped_refptr<policy::SchemaMap> schema_map =
      extension_profile->GetOriginalProfile()
          ->GetPolicySchemaRegistryService()
          ->registry()
          ->schema_map();

  const extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(extension_profile);
  const extensions::ExtensionSet extension_set =
      registry->GenerateInstalledExtensionsSet();

  for (const auto& extension : extension_set) {
    // Skip this extension if it's not an enterprise extension.
    if (!ContainsStorageManagedSchema(extension.get())) {
      continue;
    }
    base::DictValue extension_value;
    extension_value.Set(policy::kNameKey, extension->name());
    const policy::Schema* schema = schema_map->GetSchema(
        policy::PolicyNamespace(policy_domain, extension->id()));
    base::ListValue policy_names;
    if (schema && schema->valid()) {
      // Get policy names from the extension's policy schema.
      for (auto prop = schema->GetPropertiesIterator(); !prop.IsAtEnd();
           prop.Advance()) {
        policy_names.Append(prop.key());
      }
    }
    extension_value.Set(policy::kPolicyNamesKey, std::move(policy_names));
    names.Set(extension->id(), std::move(extension_value));
  }
  return names;
}

policy::PolicyGroupNamesMojoMap
ExtensionPoliciesValueProvider::GetExtensionPolicyNamesMojo(
    policy::PolicyDomain policy_domain) const {
  policy::PolicyGroupNamesMojoMap names;
  Profile* extension_profile =
      GetExtensionProfile(profile_.get(), policy_domain);

  const policy::SchemaRegistryService* schema_registry_service =
      extension_profile->GetOriginalProfile()->GetPolicySchemaRegistryService();
  if (!schema_registry_service) {
    LOG_POLICY(ERROR, POLICY_PROCESSING)
        << "Cannot get extension policy names, no schema registry service";
    return names;
  }
  const scoped_refptr<policy::SchemaMap>& schema_map =
      schema_registry_service->registry()->schema_map();

  const extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(extension_profile);
  if (!registry) {
    LOG_POLICY(ERROR, POLICY_PROCESSING)
        << "Cannot get extension policy names, no extension registry.";
    return names;
  }
  const extensions::ExtensionSet extension_set =
      registry->GenerateInstalledExtensionsSet();

  for (const auto& extension : extension_set) {
    // Skip this extension if it's not an enterprise extension.
    if (!ContainsStorageManagedSchema(extension.get())) {
      continue;
    }
    const policy::Schema* schema = schema_map->GetSchema(
        policy::PolicyNamespace(policy_domain, extension->id()));
    std::vector<std::string> policy_names;
    if (schema && schema->valid()) {
      // Get policy names from the extension's policy schema.
      for (auto prop = schema->GetPropertiesIterator(); !prop.IsAtEnd();
           prop.Advance()) {
        policy_names.emplace_back(prop.key());
      }
    }
    names.insert_or_assign(extension->id(),
                           policy::mojom::PolicyGroupNames::New(
                               extension->name(), std::move(policy_names)));
  }
  return names;
}

void ExtensionPoliciesValueProvider::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  // Notify value change if the loaded extension has policy.
  if (ContainsStorageManagedSchema(extension)) {
    NotifyValueChange();
  }
}

void ExtensionPoliciesValueProvider::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  // Notify value change if the unloaded extension has policy.
  if (ContainsStorageManagedSchema(extension)) {
    NotifyValueChange();
  }
}

void ExtensionPoliciesValueProvider::OnPolicyUpdated(
    const policy::PolicyNamespace& ns,
    const policy::PolicyMap& previous,
    const policy::PolicyMap& current) {
  NotifyValueChange();
}
