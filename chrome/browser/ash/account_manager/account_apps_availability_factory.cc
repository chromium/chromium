// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/account_manager/account_apps_availability_factory.h"

#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/account_manager/account_apps_availability.h"
#include "chrome/browser/ash/account_manager/account_manager_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/account_manager_core/chromeos/account_manager_facade_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

namespace ash {

// static
AccountAppsAvailabilityFactory* AccountAppsAvailabilityFactory::GetInstance() {
  static base::NoDestructor<AccountAppsAvailabilityFactory> factory;
  return factory.get();
}

// static
AccountAppsAvailability* AccountAppsAvailabilityFactory::GetForProfile(
    Profile* profile) {
  return static_cast<AccountAppsAvailability*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

AccountAppsAvailabilityFactory::AccountAppsAvailabilityFactory()
    : ProfileKeyedServiceFactory(
          "AccountAppsAvailability",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

AccountAppsAvailabilityFactory::~AccountAppsAvailabilityFactory() = default;

std::unique_ptr<KeyedService>
AccountAppsAvailabilityFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  DCHECK(profile);
  if (!IsAccountManagerAvailable(profile))
    return nullptr;

  if (!AccountAppsAvailability::IsArcAccountRestrictionsEnabled() &&
      !AccountAppsAvailability::IsArcManagedAccountRestrictionEnabled()) {
    return nullptr;
  }

  return std::make_unique<AccountAppsAvailability>(
      ::GetAccountManagerFacade(profile->GetPath().value()),
      IdentityManagerFactory::GetForProfile(profile), profile->GetPrefs());
}

bool AccountAppsAvailabilityFactory::ServiceIsCreatedWithBrowserContext()
    const {
  // To ensure the class is always tracking accounts in Account Manager.
  return true;
}

}  // namespace ash
