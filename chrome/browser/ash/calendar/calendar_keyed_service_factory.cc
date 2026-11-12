// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/calendar/calendar_keyed_service_factory.h"

#include "base/check_is_test.h"
#include "base/no_destructor.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
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
              .WithGuest(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
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
  if (!user) {
    CHECK_IS_TEST();
    return nullptr;
  }

  if (!user->HasGaiaAccount()) {
    return nullptr;
  }

  // The service is created only for the regular user's original Profile,
  // so AppService is always available.
  return std::make_unique<CalendarKeyedService>(
      user->GetAccountId(), profile->GetPrefs(),
      &apps::AppServiceProxyFactory::GetForProfile(profile)->AppRegistryCache(),
      AshPolicyBlocklistServiceFactory::GetForBrowserContext(profile),
      IdentityManagerFactory::GetForProfile(profile),
      profile->GetURLLoaderFactory());
}

}  // namespace ash
