// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/chrome_policy_conversions_client.h"

#include <set>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/schema_registry_service.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/browser/configuration_policy_handler_list.h"
#include "components/policy/core/browser/policy_conversions_client.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/chromeos/policy/active_directory_policy_manager.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_store_chromeos.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/device_local_account_policy_service.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#endif

using base::Value;

namespace policy {

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
Value GetIdentityFieldsFromPolicy(
    const enterprise_management::PolicyData* policy) {
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

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

ChromePolicyConversionsClient::ChromePolicyConversionsClient(
    content::BrowserContext* context) {
  DCHECK(context);
  profile_ = Profile::FromBrowserContext(
      chrome::GetBrowserContextRedirectedInIncognito(context));
}

ChromePolicyConversionsClient::~ChromePolicyConversionsClient() = default;

PolicyService* ChromePolicyConversionsClient::GetPolicyService() const {
  return profile_->GetProfilePolicyConnector()->policy_service();
}

SchemaRegistry* ChromePolicyConversionsClient::GetPolicySchemaRegistry() const {
  auto* schema_registry_service = profile_->GetPolicySchemaRegistryService();
  if (schema_registry_service) {
    return schema_registry_service->registry();
  }
  return nullptr;
}

const ConfigurationPolicyHandlerList*
ChromePolicyConversionsClient::GetHandlerList() const {
  return g_browser_process->browser_policy_connector()->GetHandlerList();
}

bool ChromePolicyConversionsClient::HasUserPolicies() const {
  return profile_ != nullptr;
}

Value ChromePolicyConversionsClient::GetExtensionPolicies(
    PolicyDomain policy_domain) {
  Value policies(Value::Type::LIST);

#if BUILDFLAG(ENABLE_EXTENSIONS)

  const bool for_signin_screen =
      policy_domain == POLICY_DOMAIN_SIGNIN_EXTENSIONS;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  Profile* extension_profile = for_signin_screen
                                   ? chromeos::ProfileHelper::GetSigninProfile()
                                   : profile_;
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
  Profile* extension_profile = profile_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  const extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(extension_profile);
  if (!registry) {
    LOG(ERROR) << "Cannot dump extension policies, no extension registry";
    return policies;
  }
  auto* schema_registry_service =
      extension_profile->GetOriginalProfile()->GetPolicySchemaRegistryService();
  if (!schema_registry_service || !schema_registry_service->registry()) {
    LOG(ERROR) << "Cannot dump extension policies, no schema registry service";
    return policies;
  }
  const scoped_refptr<SchemaMap> schema_map =
      schema_registry_service->registry()->schema_map();
  std::unique_ptr<extensions::ExtensionSet> extension_set =
      registry->GenerateInstalledExtensionsSet();
  for (const scoped_refptr<const extensions::Extension>& extension :
       *extension_set) {
    // Skip this extension if it's not an enterprise extension.
    if (!extension->manifest()->HasPath(
            extensions::manifest_keys::kStorageManagedSchema)) {
      continue;
    }

    PolicyNamespace policy_namespace =
        PolicyNamespace(policy_domain, extension->id());
    PolicyErrorMap empty_error_map;
    Value extension_policies =
        GetPolicyValues(extension_profile->GetProfilePolicyConnector()
                            ->policy_service()
                            ->GetPolicies(policy_namespace),
                        &empty_error_map, PoliciesSet(), PoliciesSet(),
                        GetKnownPolicies(schema_map, policy_namespace));
    Value extension_policies_data(Value::Type::DICTIONARY);
    extension_policies_data.SetKey("name", Value(extension->name()));
    extension_policies_data.SetKey("id", Value(extension->id()));
    extension_policies_data.SetKey("forSigninScreen", Value(for_signin_screen));
    extension_policies_data.SetKey("policies", std::move(extension_policies));
    policies.Append(std::move(extension_policies_data));
  }
#endif
  return policies;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
Value ChromePolicyConversionsClient::GetDeviceLocalAccountPolicies() {
  Value policies(Value::Type::LIST);
  // DeviceLocalAccount policies are only available for affiliated users and for
  // system logs.
  if (!GetDeviceLocalAccountPoliciesEnabled() &&
      (!user_manager::UserManager::IsInitialized() ||
       !user_manager::UserManager::Get()->GetPrimaryUser() ||
       !user_manager::UserManager::Get()->GetPrimaryUser()->IsAffiliated())) {
    return policies;
  }

  // Always includes user policies for device local account policies.
  bool current_user_policies_enabled = GetUserPoliciesEnabled();
  EnableUserPolicies(true);

  BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  DCHECK(connector);  // always not-null.

  auto* device_local_account_policy_service =
      connector->GetDeviceLocalAccountPolicyService();
  DCHECK(device_local_account_policy_service);  // always non null for
                                                // affiliated users.
  std::vector<DeviceLocalAccount> device_local_accounts =
      GetDeviceLocalAccounts(ash::CrosSettings::Get());
  for (const auto& account : device_local_accounts) {
    const std::string user_id = account.user_id;

    auto* device_local_account_policy_broker =
        device_local_account_policy_service->GetBrokerForUser(user_id);
    if (!device_local_account_policy_broker) {
      LOG(ERROR)
          << "Cannot get policy broker for device local account with user id: "
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
    PoliciesSet deprecated_policies;
    PoliciesSet future_policies;
    handler_list->ApplyPolicySettings(map, nullptr, &errors,
                                      &deprecated_policies, &future_policies);

    // Convert dictionary values to strings for display.
    handler_list->PrepareForDisplaying(&map);

    Value current_account_policies =
        GetPolicyValues(map, &errors, deprecated_policies, future_policies,
                        GetKnownPolicies(schema_map, policy_namespace));
    Value current_account_policies_data(Value::Type::DICTIONARY);
    current_account_policies_data.SetKey("id", Value(user_id));
    current_account_policies_data.SetKey("user_id", Value(user_id));
    current_account_policies_data.SetKey("name", Value(user_id));
    current_account_policies_data.SetKey("policies",
                                         std::move(current_account_policies));
    policies.Append(std::move(current_account_policies_data));
  }

  // Reset user_policies_enabled setup.
  EnableUserPolicies(current_user_policies_enabled);

  return policies;
}

Value ChromePolicyConversionsClient::GetIdentityFields() {
  Value identity_fields(Value::Type::DICTIONARY);
  if (!GetDeviceInfoEnabled())
    return Value();
  BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (!connector) {
    LOG(ERROR) << "Cannot dump identity fields, no policy connector";
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

}  // namespace policy
