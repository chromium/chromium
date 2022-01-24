// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_manager_factory.h"

#include "base/feature_list.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
SigninManagerFactory* SigninManagerFactory::GetInstance() {
  return base::Singleton<SigninManagerFactory>::get();
}

// static
SigninManager* SigninManagerFactory::GetForProfile(Profile* profile) {
  DCHECK(profile);
  return static_cast<SigninManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

SigninManagerFactory::SigninManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "SigninManager",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

SigninManagerFactory::~SigninManagerFactory() = default;

KeyedService* SigninManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // On Lacros with Mirror, signed-out profiles are not supported yet. Disable
  // the `SigninManager` so that it does not remove the primary account.
  // TODO(https://crbug.com/1259872): Revisit this once Dice is no longer
  // supported on Lacros, and see if the SigninManager can be removed from the
  // build entirely.
  if (base::FeatureList::IsEnabled(kMultiProfileAccountConsistency))
    return nullptr;
#endif

  Profile* profile = Profile::FromBrowserContext(context);
  return new SigninManager(profile->GetPrefs(),
                           IdentityManagerFactory::GetForProfile(profile));
}

bool SigninManagerFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool SigninManagerFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
