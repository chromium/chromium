// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/schema_registry_service_builder.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/policy/schema_registry_service.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/core/common/schema_registry.h"
#include "content/public/browser/browser_context.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/core/device_local_account_policy_service.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chrome/browser/profiles/profile.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#endif

namespace policy {

#if BUILDFLAG(IS_CHROMEOS_ASH)
namespace {

DeviceLocalAccountPolicyBroker* GetBroker(content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);

  if (ash::ProfileHelper::IsSigninProfile(profile))
    return nullptr;

  if (!user_manager::UserManager::IsInitialized()) {
    // Bail out in unit tests that don't have a UserManager.
    return nullptr;
  }

  const user_manager::User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user)
    return nullptr;

  BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  DeviceLocalAccountPolicyService* service =
      connector->GetDeviceLocalAccountPolicyService();
  if (!service)
    return nullptr;

  return service->GetBrokerForUser(user->GetAccountId().GetUserEmail());
}

}  // namespace
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

std::unique_ptr<SchemaRegistryService> BuildSchemaRegistryServiceForProfile(
    content::BrowserContext* context,
    const Schema& chrome_schema,
    CombinedSchemaRegistry* global_registry) {
  DCHECK(!context->IsOffTheRecord());

  std::unique_ptr<SchemaRegistry> registry;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  DeviceLocalAccountPolicyBroker* broker = GetBroker(context);
  if (broker) {
    // The SchemaRegistry for a device-local account is owned by its
    // DeviceLocalAccountPolicyBroker, which uses the registry to fetch and
    // cache policy even if there is no active session for that account.
    // Use a ForwardingSchemaRegistry that wraps this SchemaRegistry.
    registry =
        std::make_unique<ForwardingSchemaRegistry>(broker->schema_registry());
  }
#endif

  if (!registry)
    registry = std::make_unique<SchemaRegistry>();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  Profile* const profile = Profile::FromBrowserContext(context);
  if (ash::ProfileHelper::IsSigninProfile(profile)) {
    // Pass the SchemaRegistry of the signin profile to the device policy
    // managers, for being used for fetching the component policies.
    BrowserPolicyConnectorAsh* connector =
        g_browser_process->platform_part()->browser_policy_connector_ash();

    policy::DeviceCloudPolicyManagerAsh* cloud_manager =
        connector->GetDeviceCloudPolicyManager();
    if (cloud_manager)
      cloud_manager->SetSigninProfileSchemaRegistry(registry.get());
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
