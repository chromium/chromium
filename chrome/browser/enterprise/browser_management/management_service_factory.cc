// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/browser_management/management_service_factory.h"

#include "base/memory/singleton.h"
#include "base/no_destructor.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/enterprise/browser_management/browser_management_service.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browsing_data/core/features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/policy/core/common/management/platform_management_service.h"
#include "content/public/browser/browser_context.h"
#include "extensions/buildflags/buildflags.h"
#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/enterprise/browser_management/browser_management_status_provider.h"
#endif

namespace policy {

// static
ManagementServiceFactory* ManagementServiceFactory::GetInstance() {
  static base::NoDestructor<ManagementServiceFactory> instance;
  return instance.get();
}

// static
ManagementService* ManagementServiceFactory::GetForPlatform() {
  auto* instance = PlatformManagementService::GetInstance();
  // This has to be done here since `DeviceManagementStatusProvider` cannot be
  // defined in `components/policy/`, also we need we need the
  // `g_browser_process->platform_part()`.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!instance->has_cros_status_provider() && g_browser_process &&
      g_browser_process->platform_part()) {
    instance->AddChromeOsStatusProvider(
        std::make_unique<DeviceManagementStatusProvider>(
            g_browser_process->platform_part()
                ->browser_policy_connector_ash()));
  }
#endif
  return instance;
}

// static
ManagementService* ManagementServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<BrowserManagementService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

ManagementServiceFactory::ManagementServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "EnterpriseManagementService",
          BrowserContextDependencyManager::GetInstance()) {}

ManagementServiceFactory::~ManagementServiceFactory() = default;

content::BrowserContext* ManagementServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

KeyedService* ManagementServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new BrowserManagementService(Profile::FromBrowserContext(context));
}

}  // namespace policy
