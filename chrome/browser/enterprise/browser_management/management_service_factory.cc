// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/browser_management/management_service_factory.h"

#include "base/no_destructor.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/enterprise/browser_management/browser_management_service.h"
#include "chrome/browser/enterprise/browser_management/browser_management_status_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/common/management/platform_management_service.h"
#include "content/public/browser/browser_context.h"
#include "extensions/buildflags/buildflags.h"

namespace policy {

// static
ManagementServiceFactory* ManagementServiceFactory::GetInstance() {
  static base::NoDestructor<ManagementServiceFactory> instance;
  return instance.get();
}

// static
ManagementService* ManagementServiceFactory::GetForPlatform() {
  auto* instance = PlatformManagementService::GetInstance();

  // Having CBCM enabled means that the device has some kind of management,
  // however we cannot here fully trust it so we give it the authority with
  // the lowest trust. Higher management trust levels will be determined by
  // the other management status providers.
  if (!instance->has_local_browser_managment_status_provider()) {
    instance->AddLocalBrowserManagementStatusProvider(
        std::make_unique<LocalBrowserManagementStatusProvider>());
  }

  // This has to be done here since `DeviceManagementStatusProvider` cannot be
  // defined in `components/policy/`, also we need we need the
  // `g_browser_process->platform_part()`.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!instance->has_cros_status_provider()) {
    instance->AddChromeOsStatusProvider(
        std::make_unique<DeviceManagementStatusProvider>());
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
    : ProfileKeyedServiceFactory(
          "EnterpriseManagementService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {}

ManagementServiceFactory::~ManagementServiceFactory() = default;

std::unique_ptr<KeyedService>
ManagementServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<BrowserManagementService>(
      Profile::FromBrowserContext(context));
}

}  // namespace policy
