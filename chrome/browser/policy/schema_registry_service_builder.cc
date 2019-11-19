// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/schema_registry_service_builder.h"

#include <utility>

#include "base/logging.h"
#include "build/build_config.h"
#include "chrome/browser/policy/schema_registry_service.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/core/common/schema_registry.h"
#include "content/public/browser/browser_context.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/chromeos/policy/active_directory_policy_manager.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/device_local_account_policy_service.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#endif

namespace policy {

#if defined(OS_CHROMEOS)
namespace {

DeviceLocalAccountPolicyBroker* GetBroker(content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);

  if (chromeos::ProfileHelper::IsSigninProfile(profile))
    return NULL;

  if (!user_manager::UserManager::IsInitialized()) {
    // Bail out in unit tests that don't have a UserManager.
    return NULL;
  }

  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user)
    return NULL;

  BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  DeviceLocalAccountPolicyService* service =
      connector->GetDeviceLocalAccountPolicyService();
  if (!service)
    return NULL;

  return service->GetBrokerForUser(user->GetAccountId().GetUserEmail());
}

}  // namespace
#endif  // OS_CHROMEOS

std::unique_ptr<SchemaRegistryService> BuildSchemaRegistryServiceForProfile(
    content::BrowserContext* context,
    const Schema& chrome_schema,
    CombinedSchemaRegistry* global_registry) {
  DCHECK(!context->IsOffTheRecord());

  std::unique_ptr<SchemaRegistry> registry;

#if defined(OS_CHROMEOS)
  DeviceLocalAccountPolicyBroker* broker = GetBroker(context);
  if (broker) {
    // The SchemaRegistry for a device-local account is owned by its
    // DeviceLocalAccountPolicyBroker, which uses the registry to fetch and
    // cache policy even if there is no active session for that account.
    // Use a ForwardingSchemaRegistry that wraps this SchemaRegistry.
    registry.reset(new ForwardingSchemaRegistry(broker->schema_registry()));
  }
#endif

  if (!registry)
    registry.reset(new SchemaRegistry);

#if defined(OS_CHROMEOS)
  Profile* const profile = Profile::FromBrowserContext(context);
  if (chromeos::ProfileHelper::IsSigninProfile(profile)) {
    // Pass the SchemaRegistry of the signin profile to the device policy
    // managers, for being used for fetching the component policies.
    BrowserPolicyConnectorChromeOS* connector =
        g_browser_process->platform_part()->browser_policy_connector_chromeos();

    policy::DeviceCloudPolicyManagerChromeOS* cloud_manager =
        connector->GetDeviceCloudPolicyManager();
    if (cloud_manager)
      cloud_manager->SetSigninProfileSchemaRegistry(registry.get());

    policy::DeviceActiveDirectoryPolicyManager* active_directory_manager =
        connector->GetDeviceActiveDirectoryPolicyManager();
    if (active_directory_manager) {
      active_directory_manager->SetSigninProfileSchemaRegistry(registry.get());
    }
  }
#endif

  return BuildSchemaRegistryService(std::move(registry), chrome_schema,
                                    global_registry);
}

std::unique_ptr<SchemaRegistryService> BuildSchemaRegistryService(
    std::unique_ptr<SchemaRegistry> registry,
    const Schema& chrome_schema,
    CombinedSchemaRegistry* global_registry) {
  return std::make_unique<SchemaRegistryService>(
      std::move(registry), chrome_schema, global_registry);
}

}  // namespace policy
