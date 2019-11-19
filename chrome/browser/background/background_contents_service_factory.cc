// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/background_contents_service_factory.h"

#include "base/command_line.h"
#include "chrome/browser/background/background_contents_service.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
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
  return base::Singleton<BackgroundContentsServiceFactory>::get();
}

BackgroundContentsServiceFactory::BackgroundContentsServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "BackgroundContentsService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(extensions::ExtensionRegistryFactory::GetInstance());
  DependsOn(extensions::ExtensionSystemFactory::GetInstance());
}

BackgroundContentsServiceFactory::~BackgroundContentsServiceFactory() {}

KeyedService* BackgroundContentsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new BackgroundContentsService(static_cast<Profile*>(profile),
                                       base::CommandLine::ForCurrentProcess());
}

void BackgroundContentsServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* user_prefs) {
  user_prefs->RegisterDictionaryPref(prefs::kRegisteredBackgroundContents);
}

content::BrowserContext*
BackgroundContentsServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

bool BackgroundContentsServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}
