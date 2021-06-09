// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/service_factory.h"

#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"

namespace ash {
namespace file_system_provider {

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
  return base::Singleton<ServiceFactory>::get();
}

ServiceFactory::ServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "Service",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(extensions::ExtensionRegistryFactory::GetInstance());
  DependsOn(extensions::ExtensionSystemFactory::GetInstance());
  DependsOn(NotificationDisplayServiceFactory::GetInstance());
}

ServiceFactory::~ServiceFactory() {}

KeyedService* ServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new Service(Profile::FromBrowserContext(profile),
                     extensions::ExtensionRegistry::Get(profile));
}

bool ServiceFactory::ServiceIsCreatedWithBrowserContext() const { return true; }

content::BrowserContext* ServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

}  // namespace file_system_provider
}  // namespace ash
