// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/value_provider/extension_policies_value_provider.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "chrome/browser/policy/chrome_policy_conversions_client.h"
#include "chrome/browser/policy/schema_registry_service.h"
#include "chrome/browser/policy/value_provider/value_provider_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/browser/policy_conversions.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/core/common/schema_map.h"
#include "components/policy/core/common/schema_registry.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

bool ContainsStorageManagedSchema(const extensions::Extension* extension) {
  return extension->manifest()->FindPath(
      extensions::manifest_keys::kStorageManagedSchema);
}

// Looks for policy::kIdKey in `policy` dictionary and adds it to
// `extension_policies` with the ID value as a key. Moves `policy` when adding.
void AddExtensionPolicyValueToDict(base::Value& policy,
                                   base::Value::Dict& extension_policies) {
  base::Value::Dict* policy_dict = policy.GetIfDict();
  if (!policy_dict)
    return;
  std::string* id = policy_dict->FindString(policy::kIdKey);
  if (!id)
    return;
  policy_dict->Remove(*id);
  extension_policies.Set(*id, std::move(policy));
}

}  // namespace

ExtensionPoliciesValueProvider::ExtensionPoliciesValueProvider(Profile* profile)
    : profile_(profile) {
  extension_registry_observation_.Observe(
      extensions::ExtensionRegistry::Get(profile_));
  policy::PolicyService* policy_service = GetPolicyService(profile_);
  policy_service->AddObserver(policy::POLICY_DOMAIN_EXTENSIONS, this);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  policy_service->AddObserver(policy::POLICY_DOMAIN_SIGNIN_EXTENSIONS, this);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

ExtensionPoliciesValueProvider::~ExtensionPoliciesValueProvider() {
  policy::PolicyService* policy_service = GetPolicyService(profile_);
  policy_service->RemoveObserver(policy::POLICY_DOMAIN_EXTENSIONS, this);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  policy_service->RemoveObserver(policy::POLICY_DOMAIN_SIGNIN_EXTENSIONS, this);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

base::Value::Dict ExtensionPoliciesValueProvider::GetValues() {
  base::Value::Dict extension_policies;
  auto client =
      std::make_unique<policy::ChromePolicyConversionsClient>(profile_);
  if (client->HasUserPolicies()) {
    for (auto& policy :
         client->GetExtensionPolicies(policy::POLICY_DOMAIN_EXTENSIONS)) {
      AddExtensionPolicyValueToDict(policy, extension_policies);
    }
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  for (auto& policy :
       client->GetExtensionPolicies(policy::POLICY_DOMAIN_SIGNIN_EXTENSIONS)) {
    AddExtensionPolicyValueToDict(policy, extension_policies);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  return extension_policies;
}

base::Value::Dict ExtensionPoliciesValueProvider::GetNames() {
  base::Value::Dict extension_policy_names =
      GetExtensionPolicyNames(policy::POLICY_DOMAIN_EXTENSIONS);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  extension_policy_names.Merge(
      GetExtensionPolicyNames(policy::POLICY_DOMAIN_SIGNIN_EXTENSIONS));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  return extension_policy_names;
}

base::Value::Dict ExtensionPoliciesValueProvider::GetExtensionPolicyNames(
    policy::PolicyDomain policy_domain) {
  base::Value::Dict names;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  Profile* extension_profile =
      policy_domain == policy::POLICY_DOMAIN_SIGNIN_EXTENSIONS
          ? ash::ProfileHelper::GetSigninProfile()
          : profile_.get();
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
  Profile* extension_profile = profile_.get();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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
    base::Value::Dict extension_value;
    extension_value.Set(policy::kNameKey, extension->name());
    const policy::Schema* schema = schema_map->GetSchema(
        policy::PolicyNamespace(policy_domain, extension->id()));
    base::Value::List policy_names;
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
