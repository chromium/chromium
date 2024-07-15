// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_api/managed_configuration_api_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/device_api/managed_configuration_api.h"
#include "chrome/browser/profiles/profile.h"
#include "components/pref_registry/pref_registry_syncable.h"

// static
ManagedConfigurationAPIFactory* ManagedConfigurationAPIFactory::GetInstance() {
  static base::NoDestructor<ManagedConfigurationAPIFactory> instance;
  return instance.get();
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
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
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
