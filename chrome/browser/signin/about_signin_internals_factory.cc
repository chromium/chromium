// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/about_signin_internals_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/account_consistency_mode_manager_factory.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/signin/core/browser/about_signin_internals.h"

AboutSigninInternalsFactory::AboutSigninInternalsFactory()
    : ProfileKeyedServiceFactory(
          "AboutSigninInternals",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(ChromeSigninClientFactory::GetInstance());
  DependsOn(SigninErrorControllerFactory::GetInstance());
  DependsOn(AccountReconcilorFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(AccountConsistencyModeManagerFactory::GetInstance());
}

AboutSigninInternalsFactory::~AboutSigninInternalsFactory() = default;

// static
AboutSigninInternals* AboutSigninInternalsFactory::GetForProfile(
    Profile* profile) {
  return static_cast<AboutSigninInternals*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
AboutSigninInternalsFactory* AboutSigninInternalsFactory::GetInstance() {
  static base::NoDestructor<AboutSigninInternalsFactory> instance;
  return instance.get();
}

void AboutSigninInternalsFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* user_prefs) {
  AboutSigninInternals::RegisterPrefs(user_prefs);
}

KeyedService* AboutSigninInternalsFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  AboutSigninInternals* service = new AboutSigninInternals(
      IdentityManagerFactory::GetForProfile(profile),
      SigninErrorControllerFactory::GetForProfile(profile),
      AccountConsistencyModeManager::GetMethodForProfile(profile),
      ChromeSigninClientFactory::GetForProfile(profile),
      AccountReconcilorFactory::GetForProfile(profile));
  return service;
}
