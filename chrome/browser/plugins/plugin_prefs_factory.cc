// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_prefs_factory.h"

#include "base/path_service.h"
#include "base/time/time.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

// static
PluginPrefsFactory* PluginPrefsFactory::GetInstance() {
  return base::Singleton<PluginPrefsFactory>::get();
}

// static
scoped_refptr<PluginPrefs> PluginPrefsFactory::GetPrefsForProfile(
    Profile* profile) {
  return static_cast<PluginPrefs*>(
      GetInstance()->GetServiceForBrowserContext(profile, true).get());
}

// static
scoped_refptr<RefcountedKeyedService>
PluginPrefsFactory::CreateForTestingProfile(content::BrowserContext* profile) {
  return static_cast<PluginPrefs*>(
      GetInstance()->BuildServiceInstanceFor(profile).get());
}

PluginPrefsFactory::PluginPrefsFactory()
    : RefcountedBrowserContextKeyedServiceFactory(
          "PluginPrefs", BrowserContextDependencyManager::GetInstance()) {
}

PluginPrefsFactory::~PluginPrefsFactory() {}

scoped_refptr<RefcountedKeyedService>
PluginPrefsFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  scoped_refptr<PluginPrefs> plugin_prefs(new PluginPrefs());
  plugin_prefs->set_profile(profile->GetOriginalProfile());
  plugin_prefs->SetPrefs(profile->GetPrefs());
  return plugin_prefs;
}

void PluginPrefsFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  base::FilePath internal_dir;
  base::PathService::Get(chrome::DIR_INTERNAL_PLUGINS, &internal_dir);
  registry->RegisterFilePathPref(prefs::kPluginsLastInternalDirectory,
                                 internal_dir);
  registry->RegisterListPref(prefs::kPluginsPluginsList);
  registry->RegisterListPref(prefs::kPluginsDisabledPlugins);
  registry->RegisterListPref(prefs::kPluginsDisabledPluginsExceptions);
  registry->RegisterListPref(prefs::kPluginsEnabledPlugins);
  registry->RegisterBooleanPref(
      prefs::kPluginsAlwaysOpenPdfExternally, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterTimePref(prefs::kPluginsDeprecationInfobarLastShown,
                             base::Time());
}

content::BrowserContext* PluginPrefsFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool PluginPrefsFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

bool PluginPrefsFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}
