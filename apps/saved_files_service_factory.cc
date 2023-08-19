// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/saved_files_service_factory.h"

#include "apps/saved_files_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_host_registry.h"
#include "extensions/browser/extensions_browser_client.h"

namespace apps {

// static
SavedFilesService* SavedFilesServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<SavedFilesService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
SavedFilesService* SavedFilesServiceFactory::GetForBrowserContextIfExists(
    content::BrowserContext* context) {
  return static_cast<SavedFilesService*>(
      GetInstance()->GetServiceForBrowserContext(context, false));
}

// static
SavedFilesServiceFactory* SavedFilesServiceFactory::GetInstance() {
  return base::Singleton<SavedFilesServiceFactory>::get();
}

SavedFilesServiceFactory::SavedFilesServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "SavedFilesService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(extensions::ExtensionHostRegistry::GetFactory());
}

SavedFilesServiceFactory::~SavedFilesServiceFactory() = default;

KeyedService* SavedFilesServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new SavedFilesService(context);
}

content::BrowserContext* SavedFilesServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Make sure that the service is created even for incognito profile. The goal
  // is to make this service available in guest sessions, where it could be used
  // when apps white-listed in guest sessions attempt to use chrome.fileSystem
  // API.
  return extensions::ExtensionsBrowserClient::Get()
      ->GetContextRedirectedToOriginal(context, /*force_guest_profile=*/true);
}

}  // namespace apps
