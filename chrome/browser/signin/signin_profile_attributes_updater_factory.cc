// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_profile_attributes_updater_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_profile_attributes_updater.h"

// static
SigninProfileAttributesUpdater*
SigninProfileAttributesUpdaterFactory::GetForProfile(Profile* profile) {
  return static_cast<SigninProfileAttributesUpdater*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SigninProfileAttributesUpdaterFactory*
SigninProfileAttributesUpdaterFactory::GetInstance() {
  static base::NoDestructor<SigninProfileAttributesUpdaterFactory> instance;
  return instance.get();
}

SigninProfileAttributesUpdaterFactory::SigninProfileAttributesUpdaterFactory()
    : ProfileKeyedServiceFactory(
          "SigninProfileAttributesUpdater",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

SigninProfileAttributesUpdaterFactory::
    ~SigninProfileAttributesUpdaterFactory() = default;

std::unique_ptr<KeyedService>
SigninProfileAttributesUpdaterFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  // Some tests don't have a ProfileManager, disable this service.
  if (!g_browser_process->profile_manager()) {
    return nullptr;
  }

  return std::make_unique<SigninProfileAttributesUpdater>(
      IdentityManagerFactory::GetForProfile(profile),
      &g_browser_process->profile_manager()->GetProfileAttributesStorage(),
      profile->GetPath(), profile->GetPrefs());
}

bool SigninProfileAttributesUpdaterFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}
