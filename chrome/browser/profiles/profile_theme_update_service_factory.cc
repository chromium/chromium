// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_theme_update_service_factory.h"

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_theme_update_service.h"
#include "chrome/browser/themes/theme_service_factory.h"

// static
ProfileThemeUpdateService* ProfileThemeUpdateServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ProfileThemeUpdateService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ProfileThemeUpdateServiceFactory*
ProfileThemeUpdateServiceFactory::GetInstance() {
  static base::NoDestructor<ProfileThemeUpdateServiceFactory> factory;
  return factory.get();
}

ProfileThemeUpdateServiceFactory::ProfileThemeUpdateServiceFactory()
    : ProfileKeyedServiceFactory(
          "ProfileThemeUpdateServiceFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(ThemeServiceFactory::GetInstance());
}

ProfileThemeUpdateServiceFactory::~ProfileThemeUpdateServiceFactory() = default;

KeyedService* ProfileThemeUpdateServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager)
    return nullptr;  // Some tests don't have a profile manager.

  Profile* profile = Profile::FromBrowserContext(context);
  return new ProfileThemeUpdateService(
      profile, &profile_manager->GetProfileAttributesStorage());
}

bool ProfileThemeUpdateServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

bool ProfileThemeUpdateServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
