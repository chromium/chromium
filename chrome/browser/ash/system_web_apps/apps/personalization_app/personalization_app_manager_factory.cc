// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_manager_factory.h"

#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_manager.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chromeos/ash/components/local_search_service/public/cpp/local_search_service_proxy_factory.h"
#include "content/public/browser/browser_context.h"

namespace ash::personalization_app {

namespace {

ProfileSelections BuildProfileSelections() {
  return ProfileSelections::Builder()
      .WithRegular(ProfileSelection::kRedirectedToOriginal)
      // Guest users can see personalization search results so that they can
      // experiment with configuring personalization settings.
      .WithGuest(ProfileSelection::kOffTheRecordOnly)
      // No need for system profile to ever interact with personalization
      // features.
      .WithSystem(ProfileSelection::kNone)
      // TODO(crbug.com/41488885): Check if this service is needed for
      // Ash Internals.
      .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
      .Build();
}

}  // namespace

// static
PersonalizationAppManager*
PersonalizationAppManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<PersonalizationAppManager*>(
      PersonalizationAppManagerFactory::GetInstance()
          ->GetServiceForBrowserContext(context,
                                        /*create=*/true));
}

// static
PersonalizationAppManagerFactory*
PersonalizationAppManagerFactory::GetInstance() {
  static base::NoDestructor<PersonalizationAppManagerFactory> instance;
  return instance.get();
}

PersonalizationAppManagerFactory::PersonalizationAppManagerFactory()
    : ProfileKeyedServiceFactory("PersonalizationAppManager",
                                 BuildProfileSelections()) {
  DependsOn(
      local_search_service::LocalSearchServiceProxyFactory::GetInstance());
}

PersonalizationAppManagerFactory::~PersonalizationAppManagerFactory() = default;

std::unique_ptr<KeyedService>
PersonalizationAppManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  DCHECK(CanSeeWallpaperOrPersonalizationApp(
      Profile::FromBrowserContext(context)));
  auto* local_search_service_proxy = local_search_service::
      LocalSearchServiceProxyFactory::GetForBrowserContext(context);
  DCHECK(local_search_service_proxy);

  return PersonalizationAppManager::Create(context,
                                           *local_search_service_proxy);
}

bool PersonalizationAppManagerFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace ash::personalization_app
