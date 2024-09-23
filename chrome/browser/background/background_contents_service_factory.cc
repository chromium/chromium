// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/background_contents_service_factory.h"

#include "base/command_line.h"
#include "chrome/browser/background/background_contents_service.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/extension_host_registry.h"
#include "extensions/browser/extension_registry_factory.h"

// static
BackgroundContentsService* BackgroundContentsServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<BackgroundContentsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
BackgroundContentsServiceFactory*
BackgroundContentsServiceFactory::GetInstance() {
  static base::NoDestructor<BackgroundContentsServiceFactory> instance;
  return instance.get();
}

BackgroundContentsServiceFactory::BackgroundContentsServiceFactory()
    : ProfileKeyedServiceFactory(
          "BackgroundContentsService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(extensions::ExtensionRegistryFactory::GetInstance());
  DependsOn(extensions::ExtensionSystemFactory::GetInstance());
  DependsOn(extensions::ExtensionHostRegistry::GetFactory());
  DependsOn(NotificationDisplayServiceFactory::GetInstance());
}

BackgroundContentsServiceFactory::~BackgroundContentsServiceFactory() = default;

std::unique_ptr<KeyedService>
BackgroundContentsServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* profile) const {
  return std::make_unique<BackgroundContentsService>(
      static_cast<Profile*>(profile));
}

void BackgroundContentsServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* user_prefs) {
  user_prefs->RegisterDictionaryPref(prefs::kRegisteredBackgroundContents);
}

bool BackgroundContentsServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}
