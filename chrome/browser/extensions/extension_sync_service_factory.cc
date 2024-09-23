// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_sync_service_factory.h"

#include "chrome/browser/extensions/account_extension_tracker.h"
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
  static base::NoDestructor<ExtensionSyncServiceFactory> instance;
  return instance.get();
}

ExtensionSyncServiceFactory::ExtensionSyncServiceFactory()
    : ProfileKeyedServiceFactory(
          "ExtensionSyncService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(extensions::AccountExtensionTracker::GetFactory());
  DependsOn(extensions::ExtensionPrefsFactory::GetInstance());
  DependsOn(extensions::ExtensionRegistryFactory::GetInstance());
  DependsOn(extensions::ExtensionSystemFactory::GetInstance());
}

ExtensionSyncServiceFactory::~ExtensionSyncServiceFactory() = default;

std::unique_ptr<KeyedService>
ExtensionSyncServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ExtensionSyncService>(
      Profile::FromBrowserContext(context));
}
