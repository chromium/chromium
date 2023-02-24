// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_api/managed_configuration_api_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/device_api/managed_configuration_api.h"
#include "chrome/browser/profiles/profile.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service_factory.h"

// static
ManagedConfigurationAPIFactory* ManagedConfigurationAPIFactory::GetInstance() {
  return base::Singleton<ManagedConfigurationAPIFactory>::get();
}

// static
ManagedConfigurationAPI* ManagedConfigurationAPIFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ManagedConfigurationAPI*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

ManagedConfigurationAPIFactory::ManagedConfigurationAPIFactory()
    : ProfileKeyedServiceFactory(
          "ManagedConfigurationAPI",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {}

ManagedConfigurationAPIFactory::~ManagedConfigurationAPIFactory() = default;

KeyedService* ManagedConfigurationAPIFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile(Profile::FromBrowserContext(context));
  ManagedConfigurationAPI* api = new ManagedConfigurationAPI(profile);
  return api;
}

void ManagedConfigurationAPIFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  ManagedConfigurationAPI::RegisterProfilePrefs(registry);
}

bool ManagedConfigurationAPIFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}
