// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/calendar/calendar_keyed_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/calendar/calendar_keyed_service.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chromeos/ash/components/policy/policy_blocklist_service/ash_policy_blocklist_service_factory.h"
#include "components/user_manager/user.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash {

// static
CalendarKeyedServiceFactory* CalendarKeyedServiceFactory::GetInstance() {
  static base::NoDestructor<CalendarKeyedServiceFactory> factory;
  return factory.get();
}

CalendarKeyedServiceFactory::CalendarKeyedServiceFactory()
    : ProfileKeyedServiceFactory(
          "CalendarKeyedService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  // LINT.IfChange(Deps)
  DependsOn(apps::AppServiceProxyFactory::GetInstance());
  DependsOn(AshPolicyBlocklistServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  // LINT.ThenChange(//chrome/browser/ash/calendar/calendar_keyed_service.h:Deps)
}

CalendarKeyedService* CalendarKeyedServiceFactory::GetService(
    content::BrowserContext* context) {
  return static_cast<CalendarKeyedService*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

std::unique_ptr<KeyedService>
  CalendarKeyedServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* const profile = Profile::FromBrowserContext(context);
  user_manager::User* user = ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user)
    return nullptr;

  if (!user->HasGaiaAccount())
    return nullptr;

  apps::AppServiceProxy* app_service_proxy =
      apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)
          ? apps::AppServiceProxyFactory::GetForProfile(profile)
          : nullptr;
  return std::make_unique<CalendarKeyedService>(
      user->GetAccountId(), profile->GetPrefs(), app_service_proxy,
      AshPolicyBlocklistServiceFactory::GetForBrowserContext(profile),
      IdentityManagerFactory::GetForProfile(profile),
      profile->GetURLLoaderFactory());
}

}  // namespace ash
