// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_prefs_factory.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

// static
PluginPrefsFactory* PluginPrefsFactory::GetInstance() {
  static base::NoDestructor<PluginPrefsFactory> instance;
  return instance.get();
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
    : RefcountedProfileKeyedServiceFactory(
          "PluginPrefs",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {}

PluginPrefsFactory::~PluginPrefsFactory() = default;

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
  registry->RegisterBooleanPref(
      prefs::kPluginsAlwaysOpenPdfExternally, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

bool PluginPrefsFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

bool PluginPrefsFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}
