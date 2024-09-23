// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/menu_manager_factory.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

namespace extensions {

// static
MenuManager* MenuManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<MenuManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
MenuManagerFactory* MenuManagerFactory::GetInstance() {
  static base::NoDestructor<MenuManagerFactory> instance;
  return instance.get();
}

// static
std::unique_ptr<KeyedService>
MenuManagerFactory::BuildServiceInstanceForTesting(
    content::BrowserContext* context) {
  return GetInstance()->BuildServiceInstanceForBrowserContext(context);
}

MenuManagerFactory::MenuManagerFactory()
    : ProfileKeyedServiceFactory(
          "MenuManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
}

MenuManagerFactory::~MenuManagerFactory() = default;

std::unique_ptr<KeyedService> MenuManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<MenuManager>(
      profile, ExtensionSystem::Get(profile)->state_store());
}

bool MenuManagerFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool MenuManagerFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace extensions
