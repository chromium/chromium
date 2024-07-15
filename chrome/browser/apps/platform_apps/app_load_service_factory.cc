// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/app_load_service_factory.h"

#include "chrome/browser/apps/platform_apps/app_load_service.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/extension_host_registry.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

namespace apps {

// static
AppLoadService* AppLoadServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<AppLoadService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

AppLoadServiceFactory* AppLoadServiceFactory::GetInstance() {
  static base::NoDestructor<AppLoadServiceFactory> instance;
  return instance.get();
}

AppLoadServiceFactory::AppLoadServiceFactory()
    : ProfileKeyedServiceFactory(
          "AppLoadService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(extensions::AppWindowRegistry::Factory::GetInstance());
  DependsOn(extensions::ExtensionPrefsFactory::GetInstance());
  DependsOn(extensions::ExtensionRegistryFactory::GetInstance());
  DependsOn(extensions::ExtensionHostRegistry::GetFactory());
  DependsOn(
      extensions::ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
}

AppLoadServiceFactory::~AppLoadServiceFactory() = default;

std::unique_ptr<KeyedService>
AppLoadServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<AppLoadService>(context);
}

bool AppLoadServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace apps
