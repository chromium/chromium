// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_sync_service_factory.h"

#include "chrome/browser/extensions/extension_sync_service.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"

// static
ExtensionSyncService* ExtensionSyncServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ExtensionSyncService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ExtensionSyncServiceFactory* ExtensionSyncServiceFactory::GetInstance() {
  return base::Singleton<ExtensionSyncServiceFactory>::get();
}

ExtensionSyncServiceFactory::ExtensionSyncServiceFactory()
    : ProfileKeyedServiceFactory(
          "ExtensionSyncService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(extensions::ExtensionPrefsFactory::GetInstance());
  DependsOn(extensions::ExtensionRegistryFactory::GetInstance());
  DependsOn(extensions::ExtensionSystemFactory::GetInstance());
}

ExtensionSyncServiceFactory::~ExtensionSyncServiceFactory() {}

KeyedService* ExtensionSyncServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ExtensionSyncService(Profile::FromBrowserContext(context));
}
