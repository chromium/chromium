// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharesheet/sharesheet_service_factory.h"

#include <memory>

#include "base/command_line.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharesheet/sharesheet_service.h"
#include "content/public/common/content_switches.h"

namespace sharesheet {

// static
SharesheetService* SharesheetServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<SharesheetService*>(
      SharesheetServiceFactory::GetInstance()->GetServiceForBrowserContext(
          profile, true /* create */));
}

// static
SharesheetServiceFactory* SharesheetServiceFactory::GetInstance() {
  static base::NoDestructor<SharesheetServiceFactory> instance;
  return instance.get();
}

SharesheetServiceFactory::SharesheetServiceFactory()
    : ProfileKeyedServiceFactory(
          "SharesheetService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // Some tests need the service to exist in guest profiles.
              .WithGuest(ProfileSelection::kOffTheRecordOnly)
              .WithSystem(ProfileSelection::kNone)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(apps::AppServiceProxyFactory::GetInstance());
}

SharesheetServiceFactory::~SharesheetServiceFactory() = default;

std::unique_ptr<KeyedService>
SharesheetServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  // When Ash is launched in guest mode, regular profiles are also guest
  // sessions. Do not create the service in this case.
  if (profile->IsRegularProfile() && profile->IsGuestSession()) {
    return nullptr;
  }

  if (ash::ProfileHelper::IsSigninProfile(profile)) {
    return nullptr;
  }

  return std::make_unique<SharesheetService>(profile);
}

bool SharesheetServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace sharesheet
