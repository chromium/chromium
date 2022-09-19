// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_extensions/system_extensions_provider_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/system_extensions/system_extensions_persistent_storage.h"
#include "chrome/browser/ash/system_extensions/system_extensions_profile_utils.h"
#include "chrome/browser/ash/system_extensions/system_extensions_provider.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/user_manager/user_manager.h"

namespace ash {

// static
SystemExtensionsProvider*
SystemExtensionsProviderFactory::GetForProfileIfExists(Profile* profile) {
  return static_cast<SystemExtensionsProvider*>(
      SystemExtensionsProviderFactory::GetInstance()
          .GetServiceForBrowserContext(profile, /*create=*/false));
}

// static
SystemExtensionsProviderFactory&
SystemExtensionsProviderFactory::GetInstance() {
  static base::NoDestructor<SystemExtensionsProviderFactory> instance;
  return *instance;
}

SystemExtensionsProviderFactory::SystemExtensionsProviderFactory()
    : BrowserContextKeyedServiceFactory(
          "SystemExtensionsProvider",
          BrowserContextDependencyManager::GetInstance()) {}

SystemExtensionsProviderFactory::~SystemExtensionsProviderFactory() = default;

KeyedService* SystemExtensionsProviderFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new SystemExtensionsProvider(Profile::FromBrowserContext(context));
}

bool SystemExtensionsProviderFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

void SystemExtensionsProviderFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  SystemExtensionsPersistentStorage::RegisterProfilePrefs(registry);
}

content::BrowserContext*
SystemExtensionsProviderFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return GetProfileForSystemExtensions(Profile::FromBrowserContext(context));
}

}  // namespace ash
