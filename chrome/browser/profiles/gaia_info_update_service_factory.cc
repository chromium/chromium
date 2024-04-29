// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/gaia_info_update_service_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/gaia_info_update_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"

GAIAInfoUpdateServiceFactory::GAIAInfoUpdateServiceFactory()
    : ProfileKeyedServiceFactory(
          "GAIAInfoUpdateService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

GAIAInfoUpdateServiceFactory::~GAIAInfoUpdateServiceFactory() = default;

// static
GAIAInfoUpdateService* GAIAInfoUpdateServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<GAIAInfoUpdateService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
GAIAInfoUpdateServiceFactory* GAIAInfoUpdateServiceFactory::GetInstance() {
  static base::NoDestructor<GAIAInfoUpdateServiceFactory> instance;
  return instance.get();
}

std::unique_ptr<KeyedService>
GAIAInfoUpdateServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  if (!g_browser_process->profile_manager())
    return nullptr;  // Some tests don't have a profile manager.

  return std::make_unique<GAIAInfoUpdateService>(
      IdentityManagerFactory::GetForProfile(profile),
      &g_browser_process->profile_manager()->GetProfileAttributesStorage(),
      *profile->GetPrefs(), profile->GetPath());
}

bool GAIAInfoUpdateServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

bool GAIAInfoUpdateServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}
