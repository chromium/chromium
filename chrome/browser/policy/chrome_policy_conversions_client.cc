// Copyright 2020 The Chromium Authors
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
#include "components/policy/core/browser/policy_conversions.h"
#include "components/policy/core/browser/policy_conversions_client.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_logger.h"
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
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_store_ash.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ash/policy/core/device_local_account_policy_service.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#endif

using base::Value;

namespace policy {

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
Value::Dict GetIdentityFieldsFromPolicy(
    const enterprise_management::PolicyData* policy) {
  Value::Dict identity_fields;
  if (!policy) {
    return identity_fields;
  }

  if (policy->has_device_id())
    identity_fields.Set("client_id", policy->device_id());

  if (policy->has_annotated_location()) {
    identity_fields.Set("device_location", policy->annotated_location());
  }

  if (policy->has_annotated_asset_id())
    identity_fields.Set("asset_id", policy->annotated_asset_id());

  if (policy->has_display_domain())
    identity_fields.Set("display_domain", policy->display_domain());

  if (policy->has_machine_name())
    identity_fields.Set("machine_name", policy->machine_name());

  return identity_fields;
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

ChromePolicyConversionsClient::ChromePolicyConversionsClient(
    content::BrowserContext* context) {
  DCHECK(context);
  profile_ = Profile::FromBrowserContext(
      GetBrowserContextRedirectedInIncognito(context));
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

Value::List ChromePolicyConversionsClient::GetExtensionPolicies(
    PolicyDomain policy_domain) {
  Value::List policies;

#if BUILDFLAG(ENABLE_EXTENSIONS)

  const bool for_signin_screen =
      policy_domain == POLICY_DOMAIN_SIGNIN_EXTENSIONS;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  Profile* extension_profile = for_signin_screen
                                   ? ash::ProfileHelper::GetSigninProfile()
                                   : profile_.get();
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
  Profile* extension_profile = profile_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  const extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(extension_profile);
  if (!registry) {
    LOG_POLICY(ERROR, POLICY_PROCESSING)
        << "Cannot dump extension policies, no extension registry";
    return policies;
  }
  auto* schema_registry_service =
      extension_profile->GetOriginalProfile()->GetPolicySchemaRegistryService();
  if (!schema_registry_service || !schema_registry_service->registry()) {
    LOG_POLICY(ERROR, POLICY_PROCESSING)
        << "Cannot dump extension policies, no schema registry service";
    return policies;
  }
  const scoped_refptr<SchemaMap> schema_map =
      schema_registry_service->registry()->schema_map();
  const extensions::ExtensionSet extension_set =
      registry->GenerateInstalledExtensionsSet();
  for (const auto& extension : extension_set) {
    // Skip this extension if it's not an enterprise extension.
    if (!extension->manifest()->FindPath(
            extensions::manifest_keys::kStorageManagedSchema)) {
      continue;
    }

    PolicyNamespace policy_namespace =
        PolicyNamespace(policy_domain, extension->id());
    PolicyErrorMap empty_error_map;
    Value::Dict extension_policies =
        GetPolicyValues(extension_profile->GetProfilePolicyConnector()
                            ->policy_service()
                            ->GetPolicies(policy_namespace),
                        &empty_error_map, PoliciesSet(), PoliciesSet(),
                        GetKnownPolicies(schema_map, policy_namespace));
    Value::Dict extension_policies_data;
    extension_policies_data.Set(policy::kNameKey, extension->name());
    extension_policies_data.Set(policy::kIdKey, extension->id());
    extension_policies_data.Set("forSigninScreen", for_signin_screen);
    extension_policies_data.Set(policy::kPoliciesKey,
                                std::move(extension_policies));
    policies.Append(std::move(extension_policies_data));
  }
#endif
  return policies;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
Value::List ChromePolicyConversionsClient::GetDeviceLocalAccountPolicies() {
  Value::List policies;
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

  BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
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
    PolicyMap map = cloud_policy_store->policy_map().Clone();

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

    Value::Dict current_account_policies =
        GetPolicyValues(map, &errors, deprecated_policies, future_policies,
                        GetKnownPolicies(schema_map, policy_namespace));
    Value::Dict current_account_policies_data;
    current_account_policies_data.Set(policy::kIdKey, user_id);
    current_account_policies_data.Set("user_id", user_id);
    current_account_policies_data.Set(policy::kNameKey, user_id);
    current_account_policies_data.Set(policy::kPoliciesKey,
                                      std::move(current_account_policies));
    policies.Append(std::move(current_account_policies_data));
  }

  // Reset user_policies_enabled setup.
  EnableUserPolicies(current_user_policies_enabled);

  return policies;
}

Value::Dict ChromePolicyConversionsClient::GetIdentityFields() {
  Value::Dict identity_fields;
  if (!GetDeviceInfoEnabled())
    return Value::Dict();
  BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  if (!connector) {
    LOG(ERROR) << "Cannot dump identity fields, no policy connector";
    return Value::Dict();
  }
  if (connector->IsDeviceEnterpriseManaged()) {
    identity_fields.Set("enrollment_domain",
                        connector->GetEnterpriseEnrollmentDomain());

    if (connector->IsCloudManaged()) {
      Value::Dict cloud_info = GetIdentityFieldsFromPolicy(
          connector->GetDeviceCloudPolicyManager()->device_store()->policy());
      identity_fields.Merge(std::move(cloud_info));
    }
  }
  return identity_fields;
}
#endif

}  // namespace policy
