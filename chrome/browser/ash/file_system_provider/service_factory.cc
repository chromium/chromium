// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/service_factory.h"

#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"

namespace ash::file_system_provider {

// static
Service* ServiceFactory::Get(content::BrowserContext* context) {
  return static_cast<Service*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
Service* ServiceFactory::FindExisting(content::BrowserContext* context) {
  return static_cast<Service*>(
      GetInstance()->GetServiceForBrowserContext(context, false));
}

ServiceFactory* ServiceFactory::GetInstance() {
  static base::NoDestructor<ServiceFactory> instance;
  return instance.get();
}

ServiceFactory::ServiceFactory()
    : ProfileKeyedServiceFactory(
          "Service",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(extensions::ExtensionRegistryFactory::GetInstance());
  DependsOn(extensions::ExtensionSystemFactory::GetInstance());
  DependsOn(NotificationDisplayServiceFactory::GetInstance());
}

ServiceFactory::~ServiceFactory() = default;

std::unique_ptr<KeyedService>
ServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* profile) const {
  return std::make_unique<Service>(Profile::FromBrowserContext(profile),
                                   extensions::ExtensionRegistry::Get(profile));
}

bool ServiceFactory::ServiceIsCreatedWithBrowserContext() const { return true; }

}  // namespace ash::file_system_provider
